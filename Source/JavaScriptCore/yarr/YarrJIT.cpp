/*
 * Copyright (C) 2009-2018 Apple Inc. All rights reserved.
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

#include "LinkBuffer.h"
#include "Options.h"
#include "VM.h"
#include "Yarr.h"
#include "YarrCanonicalize.h"
#include "YarrDisassembler.h"
#include <wtf/ASCIICType.h>
#include <wtf/Threading.h>


#if ENABLE(YARR_JIT)

namespace JSC { namespace Yarr {

#if CPU(ARM64E)
JSC_ANNOTATE_JIT_OPERATION(_JITTarget_vmEntryToYarrJITAfter, vmEntryToYarrJITAfter);
#endif

MatchingContextHolder::MatchingContextHolder(VM& vm, YarrCodeBlock* yarrCodeBlock, MatchFrom matchFrom)
    : m_vm(vm)
{
    if (matchFrom == MatchFrom::VMThread)
        m_stackLimit = vm.softStackLimit();
    else {
        StackBounds stack = Thread::current().stack();
        m_stackLimit = stack.recursionLimit(Options::reservedZoneSize());
    }

#if ENABLE(YARR_JIT_ALL_PARENS_EXPRESSIONS)
    if (yarrCodeBlock->usesPatternContextBuffer()) {
        m_patternContextBuffer = m_vm.acquireRegExpPatternContexBuffer();
        m_patternContextBufferSize = VM::patternContextBufferSize;
    }
#else
    UNUSED_PARAM(yarrCodeBlock);
#endif
}

MatchingContextHolder::~MatchingContextHolder()
{
#if ENABLE(YARR_JIT_ALL_PARENS_EXPRESSIONS)
    if (m_patternContextBuffer)
        m_vm.releaseRegExpPatternContexBuffer();
#endif
}

template<YarrJITCompileMode compileMode>
class YarrGenerator final : public YarrJITInfo, private MacroAssembler {

#if CPU(ARM_THUMB2)
    static const RegisterID input = ARMRegisters::r0;
    static const RegisterID index = ARMRegisters::r1;
    static const RegisterID length = ARMRegisters::r2;
    static const RegisterID output = ARMRegisters::r3;

    static const RegisterID regT0 = ARMRegisters::r4;
    static const RegisterID regT1 = ARMRegisters::r5;
    static const RegisterID initialStart = ARMRegisters::r8;

    static const RegisterID returnRegister = ARMRegisters::r0;
    static const RegisterID returnRegister2 = ARMRegisters::r1;

#elif CPU(ARM64)
    // Argument registers
    static const RegisterID input = ARM64Registers::x0;
    static const RegisterID index = ARM64Registers::x1;
    static const RegisterID length = ARM64Registers::x2;
    static const RegisterID output = ARM64Registers::x3;
    static const RegisterID matchingContext = ARM64Registers::x4;
    static const RegisterID freelistRegister = ARM64Registers::x4; // Loaded from the MatchingContextHolder in the prologue.
    static const RegisterID freelistSizeRegister = ARM64Registers::x5; // Only used during initialization.

    // Scratch registers
    static const RegisterID regT0 = ARM64Registers::x6;
    static const RegisterID regT1 = ARM64Registers::x7;
    static const RegisterID regT2 = ARM64Registers::x8;
    static const RegisterID remainingMatchCount = ARM64Registers::x9;
    static const RegisterID regUnicodeInputAndTrail = ARM64Registers::x10;
    static const RegisterID unicodeTemp = ARM64Registers::x5;
    static const RegisterID initialStart = ARM64Registers::x11;
    static const RegisterID supplementaryPlanesBase = ARM64Registers::x12;
    static const RegisterID leadingSurrogateTag = ARM64Registers::x13;
    static const RegisterID trailingSurrogateTag = ARM64Registers::x14;
    static const RegisterID endOfStringAddress = ARM64Registers::x15;

    static const RegisterID returnRegister = ARM64Registers::x0;
    static const RegisterID returnRegister2 = ARM64Registers::x1;

    const TrustedImm32 surrogateTagMask = TrustedImm32(0xfffffc00);
#define JIT_UNICODE_EXPRESSIONS
#elif CPU(MIPS)
    static const RegisterID input = MIPSRegisters::a0;
    static const RegisterID index = MIPSRegisters::a1;
    static const RegisterID length = MIPSRegisters::a2;
    static const RegisterID output = MIPSRegisters::a3;

    static const RegisterID regT0 = MIPSRegisters::t4;
    static const RegisterID regT1 = MIPSRegisters::t5;
    static const RegisterID initialStart = MIPSRegisters::t6;

    static const RegisterID returnRegister = MIPSRegisters::v0;
    static const RegisterID returnRegister2 = MIPSRegisters::v1;

#elif CPU(X86_64)
#if !OS(WINDOWS)
    // Argument registers
    static const RegisterID input = X86Registers::edi;
    static const RegisterID index = X86Registers::esi;
    static const RegisterID length = X86Registers::edx;
    static const RegisterID output = X86Registers::ecx;
    static const RegisterID matchingContext = X86Registers::r8;
    static const RegisterID freelistRegister = X86Registers::r8; // Loaded from the MatchingContextHolder in the prologue.
    static const RegisterID freelistSizeRegister = X86Registers::r9; // Only used during initialization.
#else
    // If the return value doesn't fit in 64bits, its destination is pointed by rcx and the parameters are shifted.
    // http://msdn.microsoft.com/en-us/library/7572ztz4.aspx
    COMPILE_ASSERT(sizeof(MatchResult) > sizeof(void*), MatchResult_does_not_fit_in_64bits);
    static const RegisterID input = X86Registers::edx;
    static const RegisterID index = X86Registers::r8;
    static const RegisterID length = X86Registers::r9;
    static const RegisterID output = X86Registers::r10;
#endif

    // Scratch registers
    static const RegisterID regT0 = X86Registers::eax;
#if !OS(WINDOWS)
    static const RegisterID regT1 = X86Registers::r9;
    static const RegisterID regT2 = X86Registers::r10;
#else
    static const RegisterID regT1 = X86Registers::ecx;
    static const RegisterID regT2 = X86Registers::edi;
#endif

    static const RegisterID initialStart = X86Registers::ebx;
#if !OS(WINDOWS)
    static const RegisterID remainingMatchCount = X86Registers::r12;
#else
    static const RegisterID remainingMatchCount = X86Registers::esi;
#endif
    static const RegisterID regUnicodeInputAndTrail = X86Registers::r13;
    static const RegisterID unicodeTemp = X86Registers::r14;
    static const RegisterID endOfStringAddress = X86Registers::r15;

    static const RegisterID returnRegister = X86Registers::eax;
    static const RegisterID returnRegister2 = X86Registers::edx;

    const TrustedImm32 supplementaryPlanesBase = TrustedImm32(0x10000);
    const TrustedImm32 leadingSurrogateTag = TrustedImm32(0xd800);
    const TrustedImm32 trailingSurrogateTag = TrustedImm32(0xdc00);
    const TrustedImm32 surrogateTagMask = TrustedImm32(0xfffffc00);
#define JIT_UNICODE_EXPRESSIONS
#endif

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
        uint32_t begin;
        uint32_t matchAmount;
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

        static ptrdiff_t nextOffset()
        {
            return offsetof(ParenContext, next);
        }

        static ptrdiff_t beginOffset()
        {
            return offsetof(ParenContext, begin);
        }

        static ptrdiff_t matchAmountOffset()
        {
            return offsetof(ParenContext, matchAmount);
        }

        static ptrdiff_t returnAddressOffset()
        {
            return offsetof(ParenContext, returnAddress);
        }

        static ptrdiff_t subpatternOffset(size_t subpattern)
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
        RegisterID parenContextPointer = regT0;
        RegisterID nextParenContextPointer = regT2;

        size_t parenContextSize = ParenContext::sizeFor(m_parenContextSizes);

        parenContextSize = WTF::roundUpToMultipleOf<sizeof(uintptr_t)>(parenContextSize);

        if (parenContextSize > VM::patternContextBufferSize) {
            m_failureReason = JITFailureReason::ParenthesisNestedTooDeep;
            return;
        }

        load32(Address(matchingContext, MatchingContextHolder::offsetOfPatternContextBufferSize()), freelistSizeRegister);
        // Note that matchingContext and freelistRegister are likely the same register.
        loadPtr(Address(matchingContext, MatchingContextHolder::offsetOfPatternContextBuffer()), freelistRegister);
        Jump emptyFreeList = branchTestPtr(Zero, freelistRegister);
        move(freelistRegister, parenContextPointer);
        addPtr(TrustedImm32(parenContextSize), freelistRegister, nextParenContextPointer);
        addPtr(freelistRegister, freelistSizeRegister);
        subPtr(TrustedImm32(parenContextSize), freelistSizeRegister);

        Label loopTop(this);
        Jump initDone = branchPtr(Above, nextParenContextPointer, freelistSizeRegister);
        storePtr(nextParenContextPointer, Address(parenContextPointer, ParenContext::nextOffset()));
        move(nextParenContextPointer, parenContextPointer);
        addPtr(TrustedImm32(parenContextSize), parenContextPointer, nextParenContextPointer);
        jump(loopTop);

        initDone.link(this);
        storePtr(TrustedImmPtr(nullptr), Address(parenContextPointer, ParenContext::nextOffset()));
        emptyFreeList.link(this);
    }

    void allocateParenContext(RegisterID result)
    {
        m_abortExecution.append(branchTestPtr(Zero, freelistRegister));
        sub32(TrustedImm32(1), remainingMatchCount);
        m_hitMatchLimit.append(branchTestPtr(Zero, remainingMatchCount));
        move(freelistRegister, result);
        loadPtr(Address(freelistRegister, ParenContext::nextOffset()), freelistRegister);
    }

    void freeParenContext(RegisterID headPtrRegister, RegisterID newHeadPtrRegister)
    {
        loadPtr(Address(headPtrRegister, ParenContext::nextOffset()), newHeadPtrRegister);
        storePtr(freelistRegister, Address(headPtrRegister, ParenContext::nextOffset()));
        move(headPtrRegister, freelistRegister);
    }

    void saveParenContext(RegisterID parenContextReg, RegisterID tempReg, unsigned firstSubpattern, unsigned lastSubpattern, unsigned subpatternBaseFrameLocation)
    {
        store32(index, Address(parenContextReg, ParenContext::beginOffset()));
        loadFromFrame(subpatternBaseFrameLocation + BackTrackInfoParentheses::matchAmountIndex(), tempReg);
        store32(tempReg, Address(parenContextReg, ParenContext::matchAmountOffset()));
        loadFromFrame(subpatternBaseFrameLocation + BackTrackInfoParentheses::returnAddressIndex(), tempReg);
        storePtr(tempReg, Address(parenContextReg, ParenContext::returnAddressOffset()));
        if (compileMode == IncludeSubpatterns) {
            for (unsigned subpattern = firstSubpattern; subpattern <= lastSubpattern; subpattern++) {
                loadPtr(Address(output, (subpattern << 1) * sizeof(unsigned)), tempReg);
                storePtr(tempReg, Address(parenContextReg, ParenContext::subpatternOffset(subpattern)));
                clearSubpatternStart(subpattern);
            }
        }
        subpatternBaseFrameLocation += YarrStackSpaceForBackTrackInfoParentheses;
        for (unsigned frameLocation = subpatternBaseFrameLocation; frameLocation < m_parenContextSizes.frameSlots(); frameLocation++) {
            loadFromFrame(frameLocation, tempReg);
            storePtr(tempReg, Address(parenContextReg, ParenContext::savedFrameOffset(m_parenContextSizes) + frameLocation * sizeof(uintptr_t)));
        }
    }

    void restoreParenContext(RegisterID parenContextReg, RegisterID tempReg, unsigned firstSubpattern, unsigned lastSubpattern, unsigned subpatternBaseFrameLocation)
    {
        load32(Address(parenContextReg, ParenContext::beginOffset()), index);
        storeToFrame(index, subpatternBaseFrameLocation + BackTrackInfoParentheses::beginIndex());
        load32(Address(parenContextReg, ParenContext::matchAmountOffset()), tempReg);
        storeToFrame(tempReg, subpatternBaseFrameLocation + BackTrackInfoParentheses::matchAmountIndex());
        loadPtr(Address(parenContextReg, ParenContext::returnAddressOffset()), tempReg);
        storeToFrame(tempReg, subpatternBaseFrameLocation + BackTrackInfoParentheses::returnAddressIndex());
        if (compileMode == IncludeSubpatterns) {
            for (unsigned subpattern = firstSubpattern; subpattern <= lastSubpattern; subpattern++) {
                loadPtr(Address(parenContextReg, ParenContext::subpatternOffset(subpattern)), tempReg);
                storePtr(tempReg, Address(output, (subpattern << 1) * sizeof(unsigned)));
            }
        }
        subpatternBaseFrameLocation += YarrStackSpaceForBackTrackInfoParentheses;
        for (unsigned frameLocation = subpatternBaseFrameLocation; frameLocation < m_parenContextSizes.frameSlots(); frameLocation++) {
            loadPtr(Address(parenContextReg, ParenContext::savedFrameOffset(m_parenContextSizes) + frameLocation * sizeof(uintptr_t)), tempReg);
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
            if ((term.type == PatternTerm::TypeCharacterClass)
                && (term.quantityType == QuantifierFixedCount)
                && (!m_decodeSurrogatePairs || (term.characterClass->hasOneCharacterSize() && !term.m_invert))
                && (nextTerm.type == PatternTerm::TypePatternCharacter)
                && (nextTerm.quantityType == QuantifierFixedCount)) {
                PatternTerm termCopy = term;
                alternative->m_terms[i] = nextTerm;
                alternative->m_terms[i + 1] = termCopy;
            }
        }
    }

    void matchCharacterClassRange(RegisterID character, JumpList& failures, JumpList& matchDest, const CharacterRange* ranges, unsigned count, unsigned* matchIndex, const UChar32* matches, unsigned matchCount)
    {
        do {
            // pick which range we're going to generate
            int which = count >> 1;
            char lo = ranges[which].begin;
            char hi = ranges[which].end;

            // check if there are any ranges or matches below lo.  If not, just jl to failure -
            // if there is anything else to check, check that first, if it falls through jmp to failure.
            if ((*matchIndex < matchCount) && (matches[*matchIndex] < lo)) {
                Jump loOrAbove = branch32(GreaterThanOrEqual, character, Imm32((unsigned short)lo));

                // generate code for all ranges before this one
                if (which)
                    matchCharacterClassRange(character, failures, matchDest, ranges, which, matchIndex, matches, matchCount);

                while ((*matchIndex < matchCount) && (matches[*matchIndex] < lo)) {
                    matchDest.append(branch32(Equal, character, Imm32((unsigned short)matches[*matchIndex])));
                    ++*matchIndex;
                }
                failures.append(jump());

                loOrAbove.link(this);
            } else if (which) {
                Jump loOrAbove = branch32(GreaterThanOrEqual, character, Imm32((unsigned short)lo));

                matchCharacterClassRange(character, failures, matchDest, ranges, which, matchIndex, matches, matchCount);
                failures.append(jump());

                loOrAbove.link(this);
            } else
                failures.append(branch32(LessThan, character, Imm32((unsigned short)lo)));

            while ((*matchIndex < matchCount) && (matches[*matchIndex] <= hi))
                ++*matchIndex;

            matchDest.append(branch32(LessThanOrEqual, character, Imm32((unsigned short)hi)));
            // fall through to here, the value is above hi.

            // shuffle along & loop around if there are any more matches to handle.
            unsigned next = which + 1;
            ranges += next;
            count -= next;
        } while (count);
    }

    void matchCharacterClass(RegisterID character, JumpList& matchDest, const CharacterClass* charClass)
    {
        if (charClass->m_table && !m_decodeSurrogatePairs) {
            ExtendedAddress tableEntry(character, reinterpret_cast<intptr_t>(charClass->m_table));
            matchDest.append(branchTest8(charClass->m_tableInverted ? Zero : NonZero, tableEntry));
            return;
        }

        JumpList unicodeFail;
        if (charClass->m_matchesUnicode.size() || charClass->m_rangesUnicode.size()) {
            JumpList isAscii;
            if (charClass->m_matches.size() || charClass->m_ranges.size())
                isAscii.append(branch32(LessThanOrEqual, character, TrustedImm32(0x7f)));

            if (charClass->m_matchesUnicode.size()) {
                for (unsigned i = 0; i < charClass->m_matchesUnicode.size(); ++i) {
                    UChar32 ch = charClass->m_matchesUnicode[i];
                    matchDest.append(branch32(Equal, character, Imm32(ch)));
                }
            }

            if (charClass->m_rangesUnicode.size()) {
                for (unsigned i = 0; i < charClass->m_rangesUnicode.size(); ++i) {
                    UChar32 lo = charClass->m_rangesUnicode[i].begin;
                    UChar32 hi = charClass->m_rangesUnicode[i].end;

                    Jump below = branch32(LessThan, character, Imm32(lo));
                    matchDest.append(branch32(LessThanOrEqual, character, Imm32(hi)));
                    below.link(this);
                }
            }

            if (charClass->m_matches.size() || charClass->m_ranges.size())
                unicodeFail = jump();
            isAscii.link(this);
        }

        if (charClass->m_ranges.size()) {
            unsigned matchIndex = 0;
            JumpList failures;
            matchCharacterClassRange(character, failures, matchDest, charClass->m_ranges.begin(), charClass->m_ranges.size(), &matchIndex, charClass->m_matches.begin(), charClass->m_matches.size());
            while (matchIndex < charClass->m_matches.size())
                matchDest.append(branch32(Equal, character, Imm32((unsigned short)charClass->m_matches[matchIndex++])));

            failures.link(this);
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
                matchDest.append(branch32(Equal, character, Imm32((unsigned short)ch)));
            }

            if (unsigned countAZaz = matchesAZaz.size()) {
                or32(TrustedImm32(32), character);
                for (unsigned i = 0; i < countAZaz; ++i)
                    matchDest.append(branch32(Equal, character, TrustedImm32(matchesAZaz[i])));
            }
        }

        if (charClass->m_matchesUnicode.size() || charClass->m_rangesUnicode.size())
            unicodeFail.link(this);
    }

#ifdef JIT_UNICODE_EXPRESSIONS
    void advanceIndexAfterCharacterClassTermMatch(const PatternTerm* term, JumpList& failuresAfterIncrementingIndex, const RegisterID character)
    {
        ASSERT(term->type == PatternTerm::TypeCharacterClass);

        if (term->isFixedWidthCharacterClass())
            add32(TrustedImm32(term->characterClass->hasNonBMPCharacters() ? 2 : 1), index);
        else {
            add32(TrustedImm32(1), index);
            Jump isBMPChar = branch32(LessThan, character, supplementaryPlanesBase);
            failuresAfterIncrementingIndex.append(atEndOfInput());
            add32(TrustedImm32(1), index);
            isBMPChar.link(this);
        }
    }
#endif

    // Jumps if input not available; will have (incorrectly) incremented already!
    Jump jumpIfNoAvailableInput(unsigned countToCheck = 0)
    {
        if (countToCheck)
            add32(Imm32(countToCheck), index);
        return branch32(Above, index, length);
    }

    Jump jumpIfAvailableInput(unsigned countToCheck)
    {
        add32(Imm32(countToCheck), index);
        return branch32(BelowOrEqual, index, length);
    }

    Jump checkNotEnoughInput(RegisterID additionalAmount)
    {
        add32(index, additionalAmount);
        return branch32(Above, additionalAmount, length);
    }

    Jump checkInput()
    {
        return branch32(BelowOrEqual, index, length);
    }

    Jump atEndOfInput()
    {
        return branch32(Equal, index, length);
    }

    Jump notAtEndOfInput()
    {
        return branch32(NotEqual, index, length);
    }

    BaseIndex negativeOffsetIndexedAddress(Checked<unsigned> negativeCharacterOffset, RegisterID tempReg, RegisterID indexReg = index)
    {
        RegisterID base = input;

        // BaseIndex() addressing can take a int32_t offset. Given that we can have a regular
        // expression that has unsigned character offsets, BaseIndex's signed offset is insufficient
        // for addressing in extreme cases where we might underflow. Therefore we check to see if
        // negativeCharacterOffset will underflow directly or after converting for 16 bit characters.
        // If so, we do our own address calculating by adjusting the base, using the result register
        // as a temp address register.
        unsigned maximumNegativeOffsetForCharacterSize = m_charSize == Char8 ? 0x7fffffff : 0x3fffffff;
        unsigned offsetAdjustAmount = 0x40000000;
        if (negativeCharacterOffset.unsafeGet() > maximumNegativeOffsetForCharacterSize) {
            base = tempReg;
            move(input, base);
            while (negativeCharacterOffset.unsafeGet() > maximumNegativeOffsetForCharacterSize) {
                subPtr(TrustedImm32(offsetAdjustAmount), base);
                if (m_charSize != Char8)
                    subPtr(TrustedImm32(offsetAdjustAmount), base);
                negativeCharacterOffset -= offsetAdjustAmount;
            }
        }

        Checked<int32_t> characterOffset(-static_cast<int32_t>(negativeCharacterOffset.unsafeGet()));

        if (m_charSize == Char8)
            return BaseIndex(input, indexReg, TimesOne, (characterOffset * static_cast<int32_t>(sizeof(char))).unsafeGet());

        return BaseIndex(input, indexReg, TimesTwo, (characterOffset * static_cast<int32_t>(sizeof(UChar))).unsafeGet());
    }

#ifdef JIT_UNICODE_EXPRESSIONS
    void tryReadUnicodeCharImpl(RegisterID resultReg)
    {
        ASSERT(m_charSize == Char16);

        JumpList notUnicode;

        load16Unaligned(regUnicodeInputAndTrail, resultReg);

        // Is the character a leading surrogate?
        and32(surrogateTagMask, resultReg, unicodeTemp);
        notUnicode.append(branch32(NotEqual, unicodeTemp, leadingSurrogateTag));

        // Is the input long enough to read a trailing surrogate?
        addPtr(TrustedImm32(2), regUnicodeInputAndTrail);
        notUnicode.append(branchPtr(AboveOrEqual, regUnicodeInputAndTrail, endOfStringAddress));

        // Is the character a trailing surrogate?
        load16Unaligned(Address(regUnicodeInputAndTrail), regUnicodeInputAndTrail);
        and32(surrogateTagMask, regUnicodeInputAndTrail, unicodeTemp);
        notUnicode.append(branch32(NotEqual, unicodeTemp, trailingSurrogateTag));

        // Combine leading and trailing surrogates to produce a code point.
        lshift32(TrustedImm32(10), resultReg);
        getEffectiveAddress(BaseIndex(resultReg, regUnicodeInputAndTrail, TimesOne, -U16_SURROGATE_OFFSET), resultReg);
        notUnicode.link(this);
    }

    void tryReadUnicodeChar(BaseIndex address, RegisterID resultReg)
    {
        ASSERT(m_charSize == Char16);

        getEffectiveAddress(address, regUnicodeInputAndTrail);

        if (resultReg == regT0)
            m_tryReadUnicodeCharacterCalls.append(nearCall());
        else
            tryReadUnicodeCharImpl(resultReg);
    }
#endif
    
    void readCharacter(Checked<unsigned> negativeCharacterOffset, RegisterID resultReg, RegisterID indexReg = index)
    {
        BaseIndex address = negativeOffsetIndexedAddress(negativeCharacterOffset, resultReg, indexReg);

        if (m_charSize == Char8)
            load8(address, resultReg);
#ifdef JIT_UNICODE_EXPRESSIONS
        else if (m_decodeSurrogatePairs)
            tryReadUnicodeChar(address, resultReg);
#endif
        else
            load16Unaligned(address, resultReg);
    }

    Jump jumpIfCharNotEquals(UChar32 ch, Checked<unsigned> negativeCharacterOffset, RegisterID character)
    {
        readCharacter(negativeCharacterOffset, character);

        // For case-insesitive compares, non-ascii characters that have different
        // upper & lower case representations are converted to a character class.
        ASSERT(!m_pattern.ignoreCase() || isASCIIAlpha(ch) || isCanonicallyUnique(ch, m_canonicalMode));
        if (m_pattern.ignoreCase() && isASCIIAlpha(ch)) {
            or32(TrustedImm32(0x20), character);
            ch |= 0x20;
        }

        return branch32(NotEqual, character, Imm32(ch));
    }
    
    void storeToFrame(RegisterID reg, unsigned frameLocation)
    {
        poke(reg, frameLocation);
    }

    void storeToFrame(TrustedImm32 imm, unsigned frameLocation)
    {
        poke(imm, frameLocation);
    }

#if CPU(ARM64) || CPU(X86_64)
    void storeToFrame(TrustedImmPtr imm, unsigned frameLocation)
    {
        poke(imm, frameLocation);
    }
#endif

    DataLabelPtr storeToFrameWithPatch(unsigned frameLocation)
    {
        return storePtrWithPatch(TrustedImmPtr(nullptr), Address(stackPointerRegister, frameLocation * sizeof(void*)));
    }

    void loadFromFrame(unsigned frameLocation, RegisterID reg)
    {
        peek(reg, frameLocation);
    }

    void loadFromFrameAndJump(unsigned frameLocation)
    {
        farJump(Address(stackPointerRegister, frameLocation * sizeof(void*)), YarrBacktrackPtrTag);
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
            addPtr(Imm32(callFrameSizeInBytes), stackPointerRegister);
    }

    void generateFailReturn()
    {
        move(TrustedImmPtr((void*)WTF::notFound), returnRegister);
        move(TrustedImm32(0), returnRegister2);
        generateReturn();
    }

    void generateJITFailReturn()
    {
        if (m_abortExecution.empty() && m_hitMatchLimit.empty())
            return;

        JumpList finishExiting;
        if (!m_abortExecution.empty()) {
            m_abortExecution.link(this);
            move(TrustedImmPtr((void*)static_cast<size_t>(-2)), returnRegister);
            finishExiting.append(jump());
        }

        if (!m_hitMatchLimit.empty()) {
            m_hitMatchLimit.link(this);
            move(TrustedImmPtr((void*)static_cast<size_t>(-1)), returnRegister);
        }

        finishExiting.link(this);
        removeCallFrame();
        move(TrustedImm32(0), returnRegister2);
        generateReturn();
    }

    // Used to record subpatterns, should only be called if compileMode is IncludeSubpatterns.
    void setSubpatternStart(RegisterID reg, unsigned subpattern)
    {
        ASSERT(subpattern);
        // FIXME: should be able to ASSERT(compileMode == IncludeSubpatterns), but then this function is conditionally NORETURN. :-(
        store32(reg, Address(output, (subpattern << 1) * sizeof(int)));
    }
    void setSubpatternEnd(RegisterID reg, unsigned subpattern)
    {
        ASSERT(subpattern);
        // FIXME: should be able to ASSERT(compileMode == IncludeSubpatterns), but then this function is conditionally NORETURN. :-(
        store32(reg, Address(output, ((subpattern << 1) + 1) * sizeof(int)));
    }
    void clearSubpatternStart(unsigned subpattern)
    {
        ASSERT(subpattern);
        // FIXME: should be able to ASSERT(compileMode == IncludeSubpatterns), but then this function is conditionally NORETURN. :-(
        store32(TrustedImm32(-1), Address(output, (subpattern << 1) * sizeof(int)));
    }

    void clearMatches(unsigned subpattern, unsigned lastSubpattern)
    {
        for (; subpattern <= lastSubpattern; subpattern++)
            clearSubpatternStart(subpattern);
    }

    // We use one of three different strategies to track the start of the current match,
    // while matching.
    // 1) If the pattern has a fixed size, do nothing! - we calculate the value lazily
    //    at the end of matching. This is irrespective of compileMode, and in this case
    //    these methods should never be called.
    // 2) If we're compiling IncludeSubpatterns, 'output' contains a pointer to an output
    //    vector, store the match start in the output vector.
    // 3) If we're compiling MatchOnly, 'output' is unused, store the match start directly
    //    in this register.
    void setMatchStart(RegisterID reg)
    {
        ASSERT(!m_pattern.m_body->m_hasFixedSize);
        if (compileMode == IncludeSubpatterns)
            store32(reg, output);
        else
            move(reg, output);
    }
    void getMatchStart(RegisterID reg)
    {
        ASSERT(!m_pattern.m_body->m_hasFixedSize);
        if (compileMode == IncludeSubpatterns)
            load32(output, reg);
        else
            move(output, reg);
    }

    enum YarrOpCode : uint8_t {
        // These nodes wrap body alternatives - those in the main disjunction,
        // rather than subpatterns or assertions. These are chained together in
        // a doubly linked list, with a 'begin' node for the first alternative,
        // a 'next' node for each subsequent alternative, and an 'end' node at
        // the end. In the case of repeating alternatives, the 'end' node also
        // has a reference back to 'begin'.
        OpBodyAlternativeBegin,
        OpBodyAlternativeNext,
        OpBodyAlternativeEnd,
        // Similar to the body alternatives, but used for subpatterns with two
        // or more alternatives.
        OpNestedAlternativeBegin,
        OpNestedAlternativeNext,
        OpNestedAlternativeEnd,
        // Used for alternatives in subpatterns where there is only a single
        // alternative (backtracking is easier in these cases), or for alternatives
        // which never need to be backtracked (those in parenthetical assertions,
        // terminal subpatterns).
        OpSimpleNestedAlternativeBegin,
        OpSimpleNestedAlternativeNext,
        OpSimpleNestedAlternativeEnd,
        // Used to wrap 'Once' subpattern matches (quantityMaxCount == 1).
        OpParenthesesSubpatternOnceBegin,
        OpParenthesesSubpatternOnceEnd,
        // Used to wrap 'Terminal' subpattern matches (at the end of the regexp).
        OpParenthesesSubpatternTerminalBegin,
        OpParenthesesSubpatternTerminalEnd,
        // Used to wrap generic captured matches
        OpParenthesesSubpatternBegin,
        OpParenthesesSubpatternEnd,
        // Used to wrap parenthetical assertions.
        OpParentheticalAssertionBegin,
        OpParentheticalAssertionEnd,
        // Wraps all simple terms (pattern characters, character classes).
        OpTerm,
        // Where an expression contains only 'once through' body alternatives
        // and no repeating ones, this op is used to return match failure.
        OpMatchFailed
    };

    // This structure is used to hold the compiled opcode information,
    // including reference back to the original PatternTerm/PatternAlternatives,
    // and JIT compilation data structures.
    struct YarrOp {
        explicit YarrOp(PatternTerm* term)
            : m_term(term)
            , m_op(OpTerm)
            , m_isDeadCode(false)
        {
        }

        explicit YarrOp(YarrOpCode op)
            : m_op(op)
            , m_isDeadCode(false)
        {
        }

        // For alternatives, this holds the PatternAlternative and doubly linked
        // references to this alternative's siblings. In the case of the
        // OpBodyAlternativeEnd node at the end of a section of repeating nodes,
        // m_nextOp will reference the OpBodyAlternativeBegin node of the first
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
        Label m_reentry;
        JumpList m_jumps;

        // Used for backtracking when the prior alternative did not consume any
        // characters but matched.
        Jump m_zeroLengthMatch;

        // This flag is used to null out the second pattern character, when
        // two are fused to match a pair together.
        bool m_isDeadCode;

        // Currently used in the case of some of the more complex management of
        // 'm_checkedOffset', to cache the offset used in this alternative, to avoid
        // recalculating it.
        Checked<unsigned> m_checkAdjust;

        // Used by OpNestedAlternativeNext/End to hold the pointer to the
        // value that will be pushed into the pattern's frame to return to,
        // upon backtracking back into the disjunction.
        DataLabelPtr m_returnAddress;
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
    public:
        BacktrackingState()
            : m_pendingFallthrough(false)
        {
        }

        // Add a jump or jumps, a return address, or set the flag indicating
        // that the current 'fallthrough' control flow requires backtracking.
        void append(const Jump& jump)
        {
            m_laterFailures.append(jump);
        }
        void append(JumpList& jumpList)
        {
            m_laterFailures.append(jumpList);
        }
        void append(const DataLabelPtr& returnAddress)
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
                Label here(assembler);
                for (unsigned i = 0; i < m_pendingReturns.size(); ++i)
                    m_backtrackRecords.append(ReturnAddressRecord(m_pendingReturns[i], here));
                m_pendingReturns.clear();
            }
            m_laterFailures.link(assembler);
            m_laterFailures.clear();
            m_pendingFallthrough = false;
        }
        void linkTo(Label label, MacroAssembler* assembler)
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
        void takeBacktracksToJumpList(JumpList& jumpList, MacroAssembler* assembler)
        {
            if (m_pendingReturns.size()) {
                Label here(assembler);
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

        // Called at the end of code generation to link all return addresses.
        void linkDataLabels(LinkBuffer& linkBuffer)
        {
            ASSERT(isEmpty());
            for (unsigned i = 0; i < m_backtrackRecords.size(); ++i)
                linkBuffer.patch(m_backtrackRecords[i].m_dataLabel, linkBuffer.locationOf<YarrBacktrackPtrTag>(m_backtrackRecords[i].m_backtrackLocation));
        }

    private:
        struct ReturnAddressRecord {
            ReturnAddressRecord(DataLabelPtr dataLabel, Label backtrackLocation)
                : m_dataLabel(dataLabel)
                , m_backtrackLocation(backtrackLocation)
            {
            }

            DataLabelPtr m_dataLabel;
            Label m_backtrackLocation;
        };

        JumpList m_laterFailures;
        bool m_pendingFallthrough;
        Vector<DataLabelPtr, 4> m_pendingReturns;
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
            const RegisterID character = regT0;

            JumpList matchDest;
            if (!term->inputPosition)
                matchDest.append(branch32(Equal, index, Imm32(m_checkedOffset.unsafeGet())));

            readCharacter(m_checkedOffset - term->inputPosition + 1, character);
            matchCharacterClass(character, matchDest, m_pattern.newlineCharacterClass());
            op.m_jumps.append(jump());

            matchDest.link(this);
        } else {
            // Erk, really should poison out these alternatives early. :-/
            if (term->inputPosition)
                op.m_jumps.append(jump());
            else
                op.m_jumps.append(branch32(NotEqual, index, Imm32(m_checkedOffset.unsafeGet())));
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
            const RegisterID character = regT0;

            JumpList matchDest;
            if (term->inputPosition == m_checkedOffset.unsafeGet())
                matchDest.append(atEndOfInput());

            readCharacter(m_checkedOffset - term->inputPosition, character);
            matchCharacterClass(character, matchDest, m_pattern.newlineCharacterClass());
            op.m_jumps.append(jump());

            matchDest.link(this);
        } else {
            if (term->inputPosition == m_checkedOffset.unsafeGet())
                op.m_jumps.append(notAtEndOfInput());
            // Erk, really should poison out these alternatives early. :-/
            else
                op.m_jumps.append(jump());
        }
    }
    void backtrackAssertionEOL(size_t opIndex)
    {
        backtrackTermDefault(opIndex);
    }

    // Also falls though on nextIsNotWordChar.
    void matchAssertionWordchar(size_t opIndex, JumpList& nextIsWordChar, JumpList& nextIsNotWordChar)
    {
        YarrOp& op = m_ops[opIndex];
        PatternTerm* term = op.m_term;

        const RegisterID character = regT0;

        if (term->inputPosition == m_checkedOffset.unsafeGet())
            nextIsNotWordChar.append(atEndOfInput());

        readCharacter(m_checkedOffset - term->inputPosition, character);

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

        const RegisterID character = regT0;

        Jump atBegin;
        JumpList matchDest;
        if (!term->inputPosition)
            atBegin = branch32(Equal, index, Imm32(m_checkedOffset.unsafeGet()));
        readCharacter(m_checkedOffset - term->inputPosition + 1, character);

        CharacterClass* wordcharCharacterClass;

        if (m_unicodeIgnoreCase)
            wordcharCharacterClass = m_pattern.wordUnicodeIgnoreCaseCharCharacterClass();
        else
            wordcharCharacterClass = m_pattern.wordcharCharacterClass();

        matchCharacterClass(character, matchDest, wordcharCharacterClass);
        if (!term->inputPosition)
            atBegin.link(this);

        // We fall through to here if the last character was not a wordchar.
        JumpList nonWordCharThenWordChar;
        JumpList nonWordCharThenNonWordChar;
        if (term->invert()) {
            matchAssertionWordchar(opIndex, nonWordCharThenNonWordChar, nonWordCharThenWordChar);
            nonWordCharThenWordChar.append(jump());
        } else {
            matchAssertionWordchar(opIndex, nonWordCharThenWordChar, nonWordCharThenNonWordChar);
            nonWordCharThenNonWordChar.append(jump());
        }
        op.m_jumps.append(nonWordCharThenNonWordChar);

        // We jump here if the last character was a wordchar.
        matchDest.link(this);
        JumpList wordCharThenWordChar;
        JumpList wordCharThenNonWordChar;
        if (term->invert()) {
            matchAssertionWordchar(opIndex, wordCharThenNonWordChar, wordCharThenWordChar);
            wordCharThenWordChar.append(jump());
        } else {
            matchAssertionWordchar(opIndex, wordCharThenWordChar, wordCharThenNonWordChar);
            // This can fall-though!
        }

        op.m_jumps.append(wordCharThenWordChar);

        nonWordCharThenWordChar.link(this);
        wordCharThenNonWordChar.link(this);
    }
    void backtrackAssertionWordBoundary(size_t opIndex)
    {
        backtrackTermDefault(opIndex);
    }

#if ENABLE(YARR_JIT_BACKREFERENCES)
    void matchBackreference(size_t opIndex, JumpList& characterMatchFails, RegisterID character, RegisterID patternIndex, RegisterID patternCharacter)
    {
        YarrOp& op = m_ops[opIndex];
        PatternTerm* term = op.m_term;
        unsigned subpatternId = term->backReferenceSubpatternId;

        Label loop(this);

        readCharacter(0, patternCharacter, patternIndex);
        readCharacter(m_checkedOffset - term->inputPosition, character);
    
        if (!m_pattern.ignoreCase())
            characterMatchFails.append(branch32(NotEqual, character, patternCharacter));
        else {
            Jump charactersMatch = branch32(Equal, character, patternCharacter);
            ExtendedAddress characterTableEntry(character, reinterpret_cast<intptr_t>(&canonicalTableLChar));
            load16(characterTableEntry, character);
            ExtendedAddress patternTableEntry(patternCharacter, reinterpret_cast<intptr_t>(&canonicalTableLChar));
            load16(patternTableEntry, patternCharacter);
            characterMatchFails.append(branch32(NotEqual, character, patternCharacter));
            charactersMatch.link(this);
        }

        add32(TrustedImm32(1), index);
        add32(TrustedImm32(1), patternIndex);

        if (m_decodeSurrogatePairs) {
            Jump isBMPChar = branch32(LessThan, character, supplementaryPlanesBase);
            add32(TrustedImm32(1), index);
            add32(TrustedImm32(1), patternIndex);
            isBMPChar.link(this);
        }

        branch32(NotEqual, patternIndex, Address(output, ((subpatternId << 1) + 1) * sizeof(int))).linkTo(loop, this);
    }

    void generateBackReference(size_t opIndex)
    {
        YarrOp& op = m_ops[opIndex];
        PatternTerm* term = op.m_term;

        if (m_pattern.ignoreCase() && m_charSize != Char8) {
            m_failureReason = JITFailureReason::BackReference;
            return;
        }

        unsigned subpatternId = term->backReferenceSubpatternId;
        unsigned parenthesesFrameLocation = term->frameLocation;

        const RegisterID characterOrTemp = regT0;
        const RegisterID patternIndex = regT1;
        const RegisterID patternTemp = regT2;

        storeToFrame(index, parenthesesFrameLocation + BackTrackInfoBackReference::beginIndex());
        if (term->quantityType != QuantifierFixedCount || term->quantityMaxCount != 1)
            storeToFrame(TrustedImm32(0), parenthesesFrameLocation + BackTrackInfoBackReference::matchAmountIndex());

        JumpList matches;

        if (term->quantityType != QuantifierNonGreedy) {
            load32(Address(output, (subpatternId << 1) * sizeof(int)), patternIndex);
            load32(Address(output, ((subpatternId << 1) + 1) * sizeof(int)), patternTemp);

            // An empty match is successful without consuming characters
            if (term->quantityType != QuantifierFixedCount || term->quantityMaxCount != 1) {
                matches.append(branch32(Equal, TrustedImm32(-1), patternIndex));
                matches.append(branch32(Equal, patternIndex, patternTemp));
            } else {
                Jump zeroLengthMatch = branch32(Equal, TrustedImm32(-1), patternIndex);
                Jump tryNonZeroMatch = branch32(NotEqual, patternIndex, patternTemp);
                zeroLengthMatch.link(this);
                storeToFrame(TrustedImm32(1), parenthesesFrameLocation + BackTrackInfoBackReference::matchAmountIndex());
                matches.append(jump());
                tryNonZeroMatch.link(this);
            }
        }

        switch (term->quantityType) {
        case QuantifierFixedCount: {
            Label outerLoop(this);

            // PatternTemp should contain pattern end index at this point
            sub32(patternIndex, patternTemp);
            op.m_jumps.append(checkNotEnoughInput(patternTemp));

            matchBackreference(opIndex, op.m_jumps, characterOrTemp, patternIndex, patternTemp);

            if (term->quantityMaxCount != 1) {
                loadFromFrame(parenthesesFrameLocation + BackTrackInfoBackReference::matchAmountIndex(), characterOrTemp);
                add32(TrustedImm32(1), characterOrTemp);
                storeToFrame(characterOrTemp, parenthesesFrameLocation + BackTrackInfoBackReference::matchAmountIndex());
                matches.append(branch32(Equal, Imm32(term->quantityMaxCount.unsafeGet()), characterOrTemp));
                load32(Address(output, (subpatternId << 1) * sizeof(int)), patternIndex);
                load32(Address(output, ((subpatternId << 1) + 1) * sizeof(int)), patternTemp);
                jump(outerLoop);
            }
            matches.link(this);
            break;
        }

        case QuantifierGreedy: {
            JumpList incompleteMatches;

            Label outerLoop(this);

            // PatternTemp should contain pattern end index at this point
            sub32(patternIndex, patternTemp);
            matches.append(checkNotEnoughInput(patternTemp));

            matchBackreference(opIndex, incompleteMatches, characterOrTemp, patternIndex, patternTemp);

            loadFromFrame(parenthesesFrameLocation + BackTrackInfoBackReference::matchAmountIndex(), characterOrTemp);
            add32(TrustedImm32(1), characterOrTemp);
            storeToFrame(characterOrTemp, parenthesesFrameLocation + BackTrackInfoBackReference::matchAmountIndex());
            if (term->quantityMaxCount != quantifyInfinite)
                matches.append(branch32(Equal, Imm32(term->quantityMaxCount.unsafeGet()), characterOrTemp));
            load32(Address(output, (subpatternId << 1) * sizeof(int)), patternIndex);
            load32(Address(output, ((subpatternId << 1) + 1) * sizeof(int)), patternTemp);

            // Store current index in frame for restoring after a partial match
            storeToFrame(index, parenthesesFrameLocation + BackTrackInfoBackReference::beginIndex());
            jump(outerLoop);

            incompleteMatches.link(this);
            loadFromFrame(parenthesesFrameLocation + BackTrackInfoBackReference::beginIndex(), index);

            matches.link(this);
            op.m_reentry = label();
            break;
        }

        case QuantifierNonGreedy: {
            JumpList incompleteMatches;

            matches.append(jump());

            op.m_reentry = label();

            load32(Address(output, (subpatternId << 1) * sizeof(int)), patternIndex);
            load32(Address(output, ((subpatternId << 1) + 1) * sizeof(int)), patternTemp);

            // An empty match is successful without consuming characters
            Jump zeroLengthMatch = branch32(Equal, TrustedImm32(-1), patternIndex);
            Jump tryNonZeroMatch = branch32(NotEqual, patternIndex, patternTemp);
            zeroLengthMatch.link(this);
            storeToFrame(TrustedImm32(1), parenthesesFrameLocation + BackTrackInfoBackReference::matchAmountIndex());
            matches.append(jump());
            tryNonZeroMatch.link(this);

            // Check if we have input remaining to match
            sub32(patternIndex, patternTemp);
            matches.append(checkNotEnoughInput(patternTemp));

            storeToFrame(index, parenthesesFrameLocation + BackTrackInfoBackReference::beginIndex());

            matchBackreference(opIndex, incompleteMatches, characterOrTemp, patternIndex, patternTemp);

            matches.append(jump());

            incompleteMatches.link(this);
            loadFromFrame(parenthesesFrameLocation + BackTrackInfoBackReference::beginIndex(), index);

            matches.link(this);
            break;
        }
        }
    }
    void backtrackBackReference(size_t opIndex)
    {
        YarrOp& op = m_ops[opIndex];
        PatternTerm* term = op.m_term;

        unsigned subpatternId = term->backReferenceSubpatternId;

        m_backtrackingState.link(this);
        op.m_jumps.link(this);

        JumpList failures;

        unsigned parenthesesFrameLocation = term->frameLocation;
        switch (term->quantityType) {
        case QuantifierFixedCount:
            loadFromFrame(parenthesesFrameLocation + BackTrackInfoBackReference::beginIndex(), index);
            break;

        case QuantifierGreedy: {
            const RegisterID matchAmount = regT0;
            const RegisterID patternStartIndex = regT1;
            const RegisterID patternEndIndexOrLen = regT2;

            loadFromFrame(parenthesesFrameLocation + BackTrackInfoBackReference::matchAmountIndex(), matchAmount);
            failures.append(branchTest32(Zero, matchAmount));

            load32(Address(output, (subpatternId << 1) * sizeof(int)), patternStartIndex);
            load32(Address(output, ((subpatternId << 1) + 1) * sizeof(int)), patternEndIndexOrLen);
            sub32(patternStartIndex, patternEndIndexOrLen);
            sub32(patternEndIndexOrLen, index);

            sub32(TrustedImm32(1), matchAmount);
            storeToFrame(matchAmount, parenthesesFrameLocation + BackTrackInfoBackReference::matchAmountIndex());
            jump(op.m_reentry);
            break;
        }

        case QuantifierNonGreedy: {
            const RegisterID matchAmount = regT0;

            failures.append(atEndOfInput());
            loadFromFrame(parenthesesFrameLocation + BackTrackInfoBackReference::matchAmountIndex(), matchAmount);
            if (term->quantityMaxCount != quantifyInfinite)
                failures.append(branch32(AboveOrEqual, Imm32(term->quantityMaxCount.unsafeGet()), matchAmount));
            add32(TrustedImm32(1), matchAmount);
            storeToFrame(matchAmount, parenthesesFrameLocation + BackTrackInfoBackReference::matchAmountIndex());
            jump(op.m_reentry);
            break;
        }
        }
        failures.link(this);
        m_backtrackingState.fallthrough();
    }
#endif

    void generatePatternCharacterOnce(size_t opIndex)
    {
        YarrOp& op = m_ops[opIndex];

        if (op.m_isDeadCode)
            return;
        
        // m_ops always ends with a OpBodyAlternativeEnd or OpMatchFailed
        // node, so there must always be at least one more node.
        ASSERT(opIndex + 1 < m_ops.size());
        YarrOp* nextOp = &m_ops[opIndex + 1];

        PatternTerm* term = op.m_term;
        UChar32 ch = term->patternCharacter;

        if (!isLatin1(ch) && (m_charSize == Char8)) {
            // Have a 16 bit pattern character and an 8 bit string - short circuit
            op.m_jumps.append(jump());
            return;
        }

        const RegisterID character = regT0;
#if CPU(X86_64) || CPU(ARM64)
        unsigned maxCharactersAtOnce = m_charSize == Char8 ? 8 : 4;
#else
        unsigned maxCharactersAtOnce = m_charSize == Char8 ? 4 : 2;
#endif
        uint64_t ignoreCaseMask = 0;
#if CPU(BIG_ENDIAN)
        uint64_t allCharacters = ch << (m_charSize == Char8 ? 24 : 16);
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
            ignoreCaseMask |= 32 << (m_charSize == Char8 ? 24 : 16);
#else
            ignoreCaseMask |= 32;
#endif
        }

        for (numberCharacters = 1; numberCharacters < maxCharactersAtOnce && nextOp->m_op == OpTerm; ++numberCharacters, nextOp = &m_ops[opIndex + numberCharacters]) {
            PatternTerm* nextTerm = nextOp->m_term;

            // YarrJIT handles decoded surrogate pair as one character if unicode flag is enabled.
            // Note that the numberCharacters become 1 while the width of the pattern character becomes 32bit in this case.
            if (nextTerm->type != PatternTerm::TypePatternCharacter
                || nextTerm->quantityType != QuantifierFixedCount
                || nextTerm->quantityMaxCount != 1
                || nextTerm->inputPosition != (startTermPosition + numberCharacters)
                || (U16_LENGTH(nextTerm->patternCharacter) != 1 && m_decodeSurrogatePairs))
                break;

            nextOp->m_isDeadCode = true;

#if CPU(BIG_ENDIAN)
            int shiftAmount = (m_charSize == Char8 ? 24 : 16) - ((m_charSize == Char8 ? 8 : 16) * numberCharacters);
#else
            int shiftAmount = (m_charSize == Char8 ? 8 : 16) * numberCharacters;
#endif

            UChar32 currentCharacter = nextTerm->patternCharacter;

            if (!isLatin1(currentCharacter) && (m_charSize == Char8)) {
                // Have a 16 bit pattern character and an 8 bit string - short circuit
                op.m_jumps.append(jump());
                return;
            }

            // For case-insesitive compares, non-ascii characters that have different
            // upper & lower case representations are converted to a character class.
            ASSERT(!m_pattern.ignoreCase() || isASCIIAlpha(currentCharacter) || isCanonicallyUnique(currentCharacter, m_canonicalMode));

            allCharacters |= (static_cast<uint64_t>(currentCharacter) << shiftAmount);

            if ((m_pattern.ignoreCase()) && (isASCIIAlpha(currentCharacter)))
                ignoreCaseMask |= 32ULL << shiftAmount;
        }

        if (m_decodeSurrogatePairs)
            op.m_jumps.append(jumpIfNoAvailableInput());

        if (m_charSize == Char8) {
            auto check1 = [&] (Checked<unsigned> offset, UChar32 characters) {
                op.m_jumps.append(jumpIfCharNotEquals(characters, offset, character));
            };

            auto check2 = [&] (Checked<unsigned> offset, uint16_t characters, uint16_t mask) {
                load16Unaligned(negativeOffsetIndexedAddress(offset, character), character);
                if (mask)
                    or32(Imm32(mask), character);
                op.m_jumps.append(branch32(NotEqual, character, Imm32(characters | mask)));
            };

            auto check4 = [&] (Checked<unsigned> offset, unsigned characters, unsigned mask) {
                if (mask) {
                    load32WithUnalignedHalfWords(negativeOffsetIndexedAddress(offset, character), character);
                    if (mask)
                        or32(Imm32(mask), character);
                    op.m_jumps.append(branch32(NotEqual, character, Imm32(characters | mask)));
                    return;
                }
                op.m_jumps.append(branch32WithUnalignedHalfWords(NotEqual, negativeOffsetIndexedAddress(offset, character), TrustedImm32(characters)));
            };

#if CPU(X86_64) || CPU(ARM64)
            auto check8 = [&] (Checked<unsigned> offset, uint64_t characters, uint64_t mask) {
                load64(negativeOffsetIndexedAddress(offset, character), character);
                if (mask)
                    or64(TrustedImm64(mask), character);
                op.m_jumps.append(branch64(NotEqual, character, TrustedImm64(characters | mask)));
            };
#endif

            switch (numberCharacters) {
            case 1:
                // Use 32bit width of allCharacters since Yarr counts surrogate pairs as one character with unicode flag.
                check1(m_checkedOffset - startTermPosition, allCharacters & 0xffffffff);
                return;
            case 2: {
                check2(m_checkedOffset - startTermPosition, allCharacters & 0xffff, ignoreCaseMask & 0xffff);
                return;
            }
            case 3: {
                check2(m_checkedOffset - startTermPosition, allCharacters & 0xffff, ignoreCaseMask & 0xffff);
                check1(m_checkedOffset - startTermPosition - 2, (allCharacters >> 16) & 0xff);
                return;
            }
            case 4: {
                check4(m_checkedOffset - startTermPosition, allCharacters & 0xffffffff, ignoreCaseMask & 0xffffffff);
                return;
            }
#if CPU(X86_64) || CPU(ARM64)
            case 5: {
                check4(m_checkedOffset - startTermPosition, allCharacters & 0xffffffff, ignoreCaseMask & 0xffffffff);
                check1(m_checkedOffset - startTermPosition - 4, (allCharacters >> 32) & 0xff);
                return;
            }
            case 6: {
                check4(m_checkedOffset - startTermPosition, allCharacters & 0xffffffff, ignoreCaseMask & 0xffffffff);
                check2(m_checkedOffset - startTermPosition - 4, (allCharacters >> 32) & 0xffff, (ignoreCaseMask >> 32) & 0xffff);
                return;
            }
            case 7: {
                check4(m_checkedOffset - startTermPosition, allCharacters & 0xffffffff, ignoreCaseMask & 0xffffffff);
                check2(m_checkedOffset - startTermPosition - 4, (allCharacters >> 32) & 0xffff, (ignoreCaseMask >> 32) & 0xffff);
                check1(m_checkedOffset - startTermPosition - 6, (allCharacters >> 48) & 0xff);
                return;
            }
            case 8: {
                check8(m_checkedOffset - startTermPosition, allCharacters, ignoreCaseMask);
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
                    load32WithUnalignedHalfWords(negativeOffsetIndexedAddress(offset, character), character);
                    if (mask)
                        or32(Imm32(mask), character);
                    op.m_jumps.append(branch32(NotEqual, character, Imm32(characters | mask)));
                    return;
                }
                op.m_jumps.append(branch32WithUnalignedHalfWords(NotEqual, negativeOffsetIndexedAddress(offset, character), TrustedImm32(characters)));
            };

#if CPU(X86_64) || CPU(ARM64)
            auto check4 = [&] (Checked<unsigned> offset, uint64_t characters, uint64_t mask) {
                load64(negativeOffsetIndexedAddress(offset, character), character);
                if (mask)
                    or64(TrustedImm64(mask), character);
                op.m_jumps.append(branch64(NotEqual, character, TrustedImm64(characters | mask)));
            };
#endif

            switch (numberCharacters) {
            case 1:
                // Use 32bit width of allCharacters since Yarr counts surrogate pairs as one character with unicode flag.
                check1(m_checkedOffset - startTermPosition, allCharacters & 0xffffffff);
                return;
            case 2:
                check2(m_checkedOffset - startTermPosition, allCharacters & 0xffffffff, ignoreCaseMask & 0xffffffff);
                return;
#if CPU(X86_64) || CPU(ARM64)
            case 3:
                check2(m_checkedOffset - startTermPosition, allCharacters & 0xffffffff, ignoreCaseMask & 0xffffffff);
                check1(m_checkedOffset - startTermPosition - 2, (allCharacters >> 32) & 0xffff);
                return;
            case 4:
                check4(m_checkedOffset - startTermPosition, allCharacters, ignoreCaseMask);
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

        const RegisterID character = regT0;
        const RegisterID countRegister = regT1;

        if (m_decodeSurrogatePairs)
            op.m_jumps.append(jumpIfNoAvailableInput());

        move(index, countRegister);
        Checked<unsigned> scaledMaxCount = term->quantityMaxCount;
        scaledMaxCount *= U_IS_BMP(ch) ? 1 : 2;
        sub32(Imm32(scaledMaxCount.unsafeGet()), countRegister);

        Label loop(this);
        readCharacter(m_checkedOffset - term->inputPosition - scaledMaxCount, character, countRegister);
        // For case-insesitive compares, non-ascii characters that have different
        // upper & lower case representations are converted to a character class.
        ASSERT(!m_pattern.ignoreCase() || isASCIIAlpha(ch) || isCanonicallyUnique(ch, m_canonicalMode));
        if (m_pattern.ignoreCase() && isASCIIAlpha(ch)) {
            or32(TrustedImm32(0x20), character);
            ch |= 0x20;
        }

        op.m_jumps.append(branch32(NotEqual, character, Imm32(ch)));
#ifdef JIT_UNICODE_EXPRESSIONS
        if (m_decodeSurrogatePairs && !U_IS_BMP(ch))
            add32(TrustedImm32(2), countRegister);
        else
#endif
            add32(TrustedImm32(1), countRegister);
        branch32(NotEqual, countRegister, index).linkTo(loop, this);
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

        const RegisterID character = regT0;
        const RegisterID countRegister = regT1;

        move(TrustedImm32(0), countRegister);

        // Unless have a 16 bit pattern character and an 8 bit string - short circuit
        if (!(!isLatin1(ch) && (m_charSize == Char8))) {
            JumpList failures;
            Label loop(this);
            failures.append(atEndOfInput());
            failures.append(jumpIfCharNotEquals(ch, m_checkedOffset - term->inputPosition, character));

            add32(TrustedImm32(1), index);
#ifdef JIT_UNICODE_EXPRESSIONS
            if (m_decodeSurrogatePairs && !U_IS_BMP(ch)) {
                Jump surrogatePairOk = notAtEndOfInput();
                sub32(TrustedImm32(1), index);
                failures.append(jump());
                surrogatePairOk.link(this);
                add32(TrustedImm32(1), index);
            }
#endif
            add32(TrustedImm32(1), countRegister);

            if (term->quantityMaxCount == quantifyInfinite)
                jump(loop);
            else
                branch32(NotEqual, countRegister, Imm32(term->quantityMaxCount.unsafeGet())).linkTo(loop, this);

            failures.link(this);
        }
        op.m_reentry = label();

        storeToFrame(countRegister, term->frameLocation + BackTrackInfoPatternCharacter::matchAmountIndex());
    }
    void backtrackPatternCharacterGreedy(size_t opIndex)
    {
        YarrOp& op = m_ops[opIndex];
        PatternTerm* term = op.m_term;

        const RegisterID countRegister = regT1;

        m_backtrackingState.link(this);

        loadFromFrame(term->frameLocation + BackTrackInfoPatternCharacter::matchAmountIndex(), countRegister);
        m_backtrackingState.append(branchTest32(Zero, countRegister));
        sub32(TrustedImm32(1), countRegister);
        if (!m_decodeSurrogatePairs || U_IS_BMP(term->patternCharacter))
            sub32(TrustedImm32(1), index);
        else
            sub32(TrustedImm32(2), index);
        jump(op.m_reentry);
    }

    void generatePatternCharacterNonGreedy(size_t opIndex)
    {
        YarrOp& op = m_ops[opIndex];
        PatternTerm* term = op.m_term;

        const RegisterID countRegister = regT1;

        move(TrustedImm32(0), countRegister);
        op.m_reentry = label();
        storeToFrame(countRegister, term->frameLocation + BackTrackInfoPatternCharacter::matchAmountIndex());
    }
    void backtrackPatternCharacterNonGreedy(size_t opIndex)
    {
        YarrOp& op = m_ops[opIndex];
        PatternTerm* term = op.m_term;
        UChar32 ch = term->patternCharacter;

        const RegisterID character = regT0;
        const RegisterID countRegister = regT1;

        m_backtrackingState.link(this);

        loadFromFrame(term->frameLocation + BackTrackInfoPatternCharacter::matchAmountIndex(), countRegister);

        // Unless have a 16 bit pattern character and an 8 bit string - short circuit
        if (!(!isLatin1(ch) && (m_charSize == Char8))) {
            JumpList nonGreedyFailures;
            nonGreedyFailures.append(atEndOfInput());
            if (term->quantityMaxCount != quantifyInfinite)
                nonGreedyFailures.append(branch32(Equal, countRegister, Imm32(term->quantityMaxCount.unsafeGet())));
            nonGreedyFailures.append(jumpIfCharNotEquals(ch, m_checkedOffset - term->inputPosition, character));

            add32(TrustedImm32(1), index);
#ifdef JIT_UNICODE_EXPRESSIONS
            if (m_decodeSurrogatePairs && !U_IS_BMP(ch)) {
                Jump surrogatePairOk = notAtEndOfInput();
                sub32(TrustedImm32(1), index);
                nonGreedyFailures.append(jump());
                surrogatePairOk.link(this);
                add32(TrustedImm32(1), index);
            }
#endif
            add32(TrustedImm32(1), countRegister);

            jump(op.m_reentry);
            nonGreedyFailures.link(this);
        }

        if (m_decodeSurrogatePairs && !U_IS_BMP(ch)) {
            // subtract countRegister*2 for non-BMP characters
            lshift32(TrustedImm32(1), countRegister);
        }

        sub32(countRegister, index);
        m_backtrackingState.fallthrough();
    }

    void generateCharacterClassOnce(size_t opIndex)
    {
        YarrOp& op = m_ops[opIndex];
        PatternTerm* term = op.m_term;

        const RegisterID character = regT0;

        if (m_decodeSurrogatePairs) {
            op.m_jumps.append(jumpIfNoAvailableInput());
            storeToFrame(index, term->frameLocation + BackTrackInfoCharacterClass::beginIndex());
        }

        JumpList matchDest;
        readCharacter(m_checkedOffset - term->inputPosition, character);
        // If we are matching the "any character" builtin class we only need to read the
        // character and don't need to match as it will always succeed.
        if (term->invert() || !term->characterClass->m_anyCharacter) {
            matchCharacterClass(character, matchDest, term->characterClass);

            if (term->invert())
                op.m_jumps.append(matchDest);
            else {
                op.m_jumps.append(jump());
                matchDest.link(this);
            }
        }
#ifdef JIT_UNICODE_EXPRESSIONS
        if (m_decodeSurrogatePairs && (!term->characterClass->hasOneCharacterSize() || term->invert())) {
            Jump isBMPChar = branch32(LessThan, character, supplementaryPlanesBase);
            op.m_jumps.append(atEndOfInput());
            add32(TrustedImm32(1), index);
            isBMPChar.link(this);
        }
#endif
    }
    void backtrackCharacterClassOnce(size_t opIndex)
    {
#ifdef JIT_UNICODE_EXPRESSIONS
        if (m_decodeSurrogatePairs) {
            YarrOp& op = m_ops[opIndex];
            PatternTerm* term = op.m_term;

            m_backtrackingState.link(this);
            loadFromFrame(term->frameLocation + BackTrackInfoCharacterClass::beginIndex(), index);
            m_backtrackingState.fallthrough();
        }
#endif
        backtrackTermDefault(opIndex);
    }

    void generateCharacterClassFixed(size_t opIndex)
    {
        YarrOp& op = m_ops[opIndex];
        PatternTerm* term = op.m_term;

        const RegisterID character = regT0;
        const RegisterID countRegister = regT1;

        if (m_decodeSurrogatePairs)
            op.m_jumps.append(jumpIfNoAvailableInput());

        move(index, countRegister);

        Checked<unsigned> scaledMaxCount = term->quantityMaxCount;

#ifdef JIT_UNICODE_EXPRESSIONS
        if (m_decodeSurrogatePairs && term->characterClass->hasOnlyNonBMPCharacters() && !term->invert())
            scaledMaxCount *= 2;
#endif
        sub32(Imm32(scaledMaxCount.unsafeGet()), countRegister);

        Label loop(this);
        JumpList matchDest;
        readCharacter(m_checkedOffset - term->inputPosition - scaledMaxCount, character, countRegister);
        // If we are matching the "any character" builtin class we only need to read the
        // character and don't need to match as it will always succeed.
        if (term->invert() || !term->characterClass->m_anyCharacter) {
            matchCharacterClass(character, matchDest, term->characterClass);

            if (term->invert())
                op.m_jumps.append(matchDest);
            else {
                op.m_jumps.append(jump());
                matchDest.link(this);
            }
        }

#ifdef JIT_UNICODE_EXPRESSIONS
        if (m_decodeSurrogatePairs) {
            if (term->isFixedWidthCharacterClass())
                add32(TrustedImm32(term->characterClass->hasNonBMPCharacters() ? 2 : 1), countRegister);
            else {
                add32(TrustedImm32(1), countRegister);
                Jump isBMPChar = branch32(LessThan, character, supplementaryPlanesBase);
                op.m_jumps.append(atEndOfInput());
                add32(TrustedImm32(1), countRegister);
                add32(TrustedImm32(1), index);
                isBMPChar.link(this);
            }
        } else
#endif
            add32(TrustedImm32(1), countRegister);
        branch32(NotEqual, countRegister, index).linkTo(loop, this);
    }
    void backtrackCharacterClassFixed(size_t opIndex)
    {
        backtrackTermDefault(opIndex);
    }

    void generateCharacterClassGreedy(size_t opIndex)
    {
        YarrOp& op = m_ops[opIndex];
        PatternTerm* term = op.m_term;

        const RegisterID character = regT0;
        const RegisterID countRegister = regT1;

        if (m_decodeSurrogatePairs && (!term->characterClass->hasOneCharacterSize() || term->invert()))
            storeToFrame(index, term->frameLocation + BackTrackInfoCharacterClass::beginIndex());
        move(TrustedImm32(0), countRegister);

        JumpList failures;
        JumpList failuresDecrementIndex;
        Label loop(this);
#ifdef JIT_UNICODE_EXPRESSIONS
        if (term->isFixedWidthCharacterClass() && term->characterClass->hasNonBMPCharacters()) {
            move(TrustedImm32(1), character);
            failures.append(checkNotEnoughInput(character));
        } else
#endif
            failures.append(atEndOfInput());

        if (term->invert()) {
            readCharacter(m_checkedOffset - term->inputPosition, character);
            matchCharacterClass(character, failures, term->characterClass);
        } else {
            JumpList matchDest;
            readCharacter(m_checkedOffset - term->inputPosition, character);
            // If we are matching the "any character" builtin class for non-unicode patterns,
            // we only need to read the character and don't need to match as it will always succeed.
            if (!term->characterClass->m_anyCharacter) {
                matchCharacterClass(character, matchDest, term->characterClass);
                failures.append(jump());
            }
            matchDest.link(this);
        }

#ifdef JIT_UNICODE_EXPRESSIONS
        if (m_decodeSurrogatePairs)
            advanceIndexAfterCharacterClassTermMatch(term, failuresDecrementIndex, character);
        else
#endif
            add32(TrustedImm32(1), index);
        add32(TrustedImm32(1), countRegister);

        if (term->quantityMaxCount != quantifyInfinite) {
            branch32(NotEqual, countRegister, Imm32(term->quantityMaxCount.unsafeGet())).linkTo(loop, this);
            failures.append(jump());
        } else
            jump(loop);

        if (!failuresDecrementIndex.empty()) {
            failuresDecrementIndex.link(this);
            sub32(TrustedImm32(1), index);
        }

        failures.link(this);
        op.m_reentry = label();

        storeToFrame(countRegister, term->frameLocation + BackTrackInfoCharacterClass::matchAmountIndex());
    }
    void backtrackCharacterClassGreedy(size_t opIndex)
    {
        YarrOp& op = m_ops[opIndex];
        PatternTerm* term = op.m_term;

        const RegisterID countRegister = regT1;

        m_backtrackingState.link(this);

        loadFromFrame(term->frameLocation + BackTrackInfoCharacterClass::matchAmountIndex(), countRegister);
        m_backtrackingState.append(branchTest32(Zero, countRegister));
        sub32(TrustedImm32(1), countRegister);
        storeToFrame(countRegister, term->frameLocation + BackTrackInfoCharacterClass::matchAmountIndex());

        if (!m_decodeSurrogatePairs)
            sub32(TrustedImm32(1), index);
        else if (term->isFixedWidthCharacterClass())
            sub32(TrustedImm32(term->characterClass->hasNonBMPCharacters() ? 2 : 1), index);
        else {
            // Rematch one less
            const RegisterID character = regT0;

            loadFromFrame(term->frameLocation + BackTrackInfoCharacterClass::beginIndex(), index);

            Label rematchLoop(this);
            Jump doneRematching = branchTest32(Zero, countRegister);

            readCharacter(m_checkedOffset - term->inputPosition, character);

            sub32(TrustedImm32(1), countRegister);
            add32(TrustedImm32(1), index);

#ifdef JIT_UNICODE_EXPRESSIONS
            Jump isBMPChar = branch32(LessThan, character, supplementaryPlanesBase);
            add32(TrustedImm32(1), index);
            isBMPChar.link(this);
#endif

            jump(rematchLoop);
            doneRematching.link(this);

            loadFromFrame(term->frameLocation + BackTrackInfoCharacterClass::matchAmountIndex(), countRegister);
        }
        jump(op.m_reentry);
    }

    void generateCharacterClassNonGreedy(size_t opIndex)
    {
        YarrOp& op = m_ops[opIndex];
        PatternTerm* term = op.m_term;

        const RegisterID countRegister = regT1;

        move(TrustedImm32(0), countRegister);
        op.m_reentry = label();

#ifdef JIT_UNICODE_EXPRESSIONS
        if (m_decodeSurrogatePairs) {
            if (!term->characterClass->hasOneCharacterSize() || term->invert())
                storeToFrame(index, term->frameLocation + BackTrackInfoCharacterClass::beginIndex());
        }
#endif

        storeToFrame(countRegister, term->frameLocation + BackTrackInfoCharacterClass::matchAmountIndex());
    }

    void backtrackCharacterClassNonGreedy(size_t opIndex)
    {
        YarrOp& op = m_ops[opIndex];
        PatternTerm* term = op.m_term;

        const RegisterID character = regT0;
        const RegisterID countRegister = regT1;

        JumpList nonGreedyFailures;
        JumpList nonGreedyFailuresDecrementIndex;

        m_backtrackingState.link(this);

#ifdef JIT_UNICODE_EXPRESSIONS
        if (m_decodeSurrogatePairs) {
            if (!term->characterClass->hasOneCharacterSize() || term->invert())
                loadFromFrame(term->frameLocation + BackTrackInfoCharacterClass::beginIndex(), index);
        }
#endif

        loadFromFrame(term->frameLocation + BackTrackInfoCharacterClass::matchAmountIndex(), countRegister);

        nonGreedyFailures.append(atEndOfInput());
        nonGreedyFailures.append(branch32(Equal, countRegister, Imm32(term->quantityMaxCount.unsafeGet())));

        JumpList matchDest;
        readCharacter(m_checkedOffset - term->inputPosition, character);
        // If we are matching the "any character" builtin class for non-unicode patterns,
        // we only need to read the character and don't need to match as it will always succeed.
        if (term->invert() || !term->characterClass->m_anyCharacter) {
            matchCharacterClass(character, matchDest, term->characterClass);

            if (term->invert())
                nonGreedyFailures.append(matchDest);
            else {
                nonGreedyFailures.append(jump());
                matchDest.link(this);
            }
        }

#ifdef JIT_UNICODE_EXPRESSIONS
        if (m_decodeSurrogatePairs)
            advanceIndexAfterCharacterClassTermMatch(term, nonGreedyFailuresDecrementIndex, character);
        else
#endif
            add32(TrustedImm32(1), index);
        add32(TrustedImm32(1), countRegister);

        jump(op.m_reentry);

        if (!nonGreedyFailuresDecrementIndex.empty()) {
            nonGreedyFailuresDecrementIndex.link(this);
            breakpoint();
        }
        nonGreedyFailures.link(this);
        sub32(countRegister, index);
        m_backtrackingState.fallthrough();
    }

    void generateDotStarEnclosure(size_t opIndex)
    {
        YarrOp& op = m_ops[opIndex];
        PatternTerm* term = op.m_term;

        const RegisterID character = regT0;
        const RegisterID matchPos = regT1;
        JumpList foundBeginningNewLine;
        JumpList saveStartIndex;
        JumpList foundEndingNewLine;

        if (m_pattern.dotAll()) {
            move(TrustedImm32(0), matchPos);
            setMatchStart(matchPos);
            move(length, index);
            return;
        }

        ASSERT(!m_pattern.m_body->m_hasFixedSize);
        getMatchStart(matchPos);

        saveStartIndex.append(branch32(BelowOrEqual, matchPos, initialStart));
        Label findBOLLoop(this);
        sub32(TrustedImm32(1), matchPos);
        if (m_charSize == Char8)
            load8(BaseIndex(input, matchPos, TimesOne, 0), character);
        else
            load16(BaseIndex(input, matchPos, TimesTwo, 0), character);
        matchCharacterClass(character, foundBeginningNewLine, m_pattern.newlineCharacterClass());

        branch32(Above, matchPos, initialStart).linkTo(findBOLLoop, this);
        saveStartIndex.append(jump());

        foundBeginningNewLine.link(this);
        add32(TrustedImm32(1), matchPos); // Advance past newline
        saveStartIndex.link(this);

        if (!m_pattern.multiline() && term->anchors.bolAnchor)
            op.m_jumps.append(branchTest32(NonZero, matchPos));

        ASSERT(!m_pattern.m_body->m_hasFixedSize);
        setMatchStart(matchPos);

        move(index, matchPos);

        Label findEOLLoop(this);        
        foundEndingNewLine.append(branch32(Equal, matchPos, length));
        if (m_charSize == Char8)
            load8(BaseIndex(input, matchPos, TimesOne, 0), character);
        else
            load16(BaseIndex(input, matchPos, TimesTwo, 0), character);
        matchCharacterClass(character, foundEndingNewLine, m_pattern.newlineCharacterClass());
        add32(TrustedImm32(1), matchPos);
        jump(findEOLLoop);

        foundEndingNewLine.link(this);

        if (!m_pattern.multiline() && term->anchors.eolAnchor)
            op.m_jumps.append(branch32(NotEqual, matchPos, length));

        move(matchPos, index);
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
        case PatternTerm::TypePatternCharacter:
            switch (term->quantityType) {
            case QuantifierFixedCount:
                if (term->quantityMaxCount == 1)
                    generatePatternCharacterOnce(opIndex);
                else
                    generatePatternCharacterFixed(opIndex);
                break;
            case QuantifierGreedy:
                generatePatternCharacterGreedy(opIndex);
                break;
            case QuantifierNonGreedy:
                generatePatternCharacterNonGreedy(opIndex);
                break;
            }
            break;

        case PatternTerm::TypeCharacterClass:
            switch (term->quantityType) {
            case QuantifierFixedCount:
                if (term->quantityMaxCount == 1)
                    generateCharacterClassOnce(opIndex);
                else
                    generateCharacterClassFixed(opIndex);
                break;
            case QuantifierGreedy:
                generateCharacterClassGreedy(opIndex);
                break;
            case QuantifierNonGreedy:
                generateCharacterClassNonGreedy(opIndex);
                break;
            }
            break;

        case PatternTerm::TypeAssertionBOL:
            generateAssertionBOL(opIndex);
            break;

        case PatternTerm::TypeAssertionEOL:
            generateAssertionEOL(opIndex);
            break;

        case PatternTerm::TypeAssertionWordBoundary:
            generateAssertionWordBoundary(opIndex);
            break;

        case PatternTerm::TypeForwardReference:
            m_failureReason = JITFailureReason::ForwardReference;
            break;

        case PatternTerm::TypeParenthesesSubpattern:
        case PatternTerm::TypeParentheticalAssertion:
            RELEASE_ASSERT_NOT_REACHED();

        case PatternTerm::TypeBackReference:
#if ENABLE(YARR_JIT_BACKREFERENCES)
            generateBackReference(opIndex);
#else
            m_failureReason = JITFailureReason::BackReference;
#endif
            break;
        case PatternTerm::TypeDotStarEnclosure:
            generateDotStarEnclosure(opIndex);
            break;
        }
    }
    void backtrackTerm(size_t opIndex)
    {
        YarrOp& op = m_ops[opIndex];
        PatternTerm* term = op.m_term;

        switch (term->type) {
        case PatternTerm::TypePatternCharacter:
            switch (term->quantityType) {
            case QuantifierFixedCount:
                if (term->quantityMaxCount == 1)
                    backtrackPatternCharacterOnce(opIndex);
                else
                    backtrackPatternCharacterFixed(opIndex);
                break;
            case QuantifierGreedy:
                backtrackPatternCharacterGreedy(opIndex);
                break;
            case QuantifierNonGreedy:
                backtrackPatternCharacterNonGreedy(opIndex);
                break;
            }
            break;

        case PatternTerm::TypeCharacterClass:
            switch (term->quantityType) {
            case QuantifierFixedCount:
                if (term->quantityMaxCount == 1)
                    backtrackCharacterClassOnce(opIndex);
                else
                    backtrackCharacterClassFixed(opIndex);
                break;
            case QuantifierGreedy:
                backtrackCharacterClassGreedy(opIndex);
                break;
            case QuantifierNonGreedy:
                backtrackCharacterClassNonGreedy(opIndex);
                break;
            }
            break;

        case PatternTerm::TypeAssertionBOL:
            backtrackAssertionBOL(opIndex);
            break;

        case PatternTerm::TypeAssertionEOL:
            backtrackAssertionEOL(opIndex);
            break;

        case PatternTerm::TypeAssertionWordBoundary:
            backtrackAssertionWordBoundary(opIndex);
            break;

        case PatternTerm::TypeForwardReference:
            m_failureReason = JITFailureReason::ForwardReference;
            break;

        case PatternTerm::TypeParenthesesSubpattern:
        case PatternTerm::TypeParentheticalAssertion:
            RELEASE_ASSERT_NOT_REACHED();

        case PatternTerm::TypeBackReference:
#if ENABLE(YARR_JIT_BACKREFERENCES)
            backtrackBackReference(opIndex);
#else
            m_failureReason = JITFailureReason::BackReference;
#endif
            break;

        case PatternTerm::TypeDotStarEnclosure:
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
                m_disassembler->setForGenerate(opIndex, label());

            YarrOp& op = m_ops[opIndex];
            switch (op.m_op) {

            case OpTerm:
                generateTerm(opIndex);
                break;

            // OpBodyAlternativeBegin/Next/End
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
            case OpBodyAlternativeBegin: {
                PatternAlternative* alternative = op.m_alternative;

                // Upon entry at the head of the set of alternatives, check if input is available
                // to run the first alternative. (This progresses the input position).
                op.m_jumps.append(jumpIfNoAvailableInput(alternative->m_minimumSize));
                // We will reenter after the check, and assume the input position to have been
                // set as appropriate to this alternative.
                op.m_reentry = label();

                m_checkedOffset += alternative->m_minimumSize;
                break;
            }
            case OpBodyAlternativeNext:
            case OpBodyAlternativeEnd: {
                PatternAlternative* priorAlternative = m_ops[op.m_previousOp].m_alternative;
                PatternAlternative* alternative = op.m_alternative;

                // If we get here, the prior alternative matched - return success.
                
                // Adjust the stack pointer to remove the pattern's frame.
                removeCallFrame();

                // Load appropriate values into the return register and the first output
                // slot, and return. In the case of pattern with a fixed size, we will
                // not have yet set the value in the first 
                ASSERT(index != returnRegister);
                if (m_pattern.m_body->m_hasFixedSize) {
                    move(index, returnRegister);
                    if (priorAlternative->m_minimumSize)
                        sub32(Imm32(priorAlternative->m_minimumSize), returnRegister);
                    if (compileMode == IncludeSubpatterns)
                        store32(returnRegister, output);
                } else
                    getMatchStart(returnRegister);
                if (compileMode == IncludeSubpatterns)
                    store32(index, Address(output, 4));
                move(index, returnRegister2);

                generateReturn();

                // This is the divide between the tail of the prior alternative, above, and
                // the head of the subsequent alternative, below.

                if (op.m_op == OpBodyAlternativeNext) {
                    // This is the reentry point for the Next alternative. We expect any code
                    // that jumps here to do so with the input position matching that of the
                    // PRIOR alteranative, and we will only check input availability if we
                    // need to progress it forwards.
                    op.m_reentry = label();
                    if (alternative->m_minimumSize > priorAlternative->m_minimumSize) {
                        add32(Imm32(alternative->m_minimumSize - priorAlternative->m_minimumSize), index);
                        op.m_jumps.append(jumpIfNoAvailableInput());
                    } else if (priorAlternative->m_minimumSize > alternative->m_minimumSize)
                        sub32(Imm32(priorAlternative->m_minimumSize - alternative->m_minimumSize), index);
                } else if (op.m_nextOp == notFound) {
                    // This is the reentry point for the End of 'once through' alternatives,
                    // jumped to when the last alternative fails to match.
                    op.m_reentry = label();
                    sub32(Imm32(priorAlternative->m_minimumSize), index);
                }

                if (op.m_op == OpBodyAlternativeNext)
                    m_checkedOffset += alternative->m_minimumSize;
                m_checkedOffset -= priorAlternative->m_minimumSize;
                break;
            }

            // OpSimpleNestedAlternativeBegin/Next/End
            // OpNestedAlternativeBegin/Next/End
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
            case OpSimpleNestedAlternativeBegin:
            case OpNestedAlternativeBegin: {
                PatternTerm* term = op.m_term;
                PatternAlternative* alternative = op.m_alternative;
                PatternDisjunction* disjunction = term->parentheses.disjunction;

                // Calculate how much input we need to check for, and if non-zero check.
                op.m_checkAdjust = Checked<unsigned>(alternative->m_minimumSize);
                if ((term->quantityType == QuantifierFixedCount) && (term->type != PatternTerm::TypeParentheticalAssertion))
                    op.m_checkAdjust -= disjunction->m_minimumSize;
                if (op.m_checkAdjust)
                    op.m_jumps.append(jumpIfNoAvailableInput(op.m_checkAdjust.unsafeGet()));

                m_checkedOffset += op.m_checkAdjust;
                break;
            }
            case OpSimpleNestedAlternativeNext:
            case OpNestedAlternativeNext: {
                PatternTerm* term = op.m_term;
                PatternAlternative* alternative = op.m_alternative;
                PatternDisjunction* disjunction = term->parentheses.disjunction;

                // In the non-simple case, store a 'return address' so we can backtrack correctly.
                if (op.m_op == OpNestedAlternativeNext) {
                    unsigned parenthesesFrameLocation = term->frameLocation;
                    op.m_returnAddress = storeToFrameWithPatch(parenthesesFrameLocation + BackTrackInfoParentheses::returnAddressIndex());
                }

                if (term->quantityType != QuantifierFixedCount && !m_ops[op.m_previousOp].m_alternative->m_minimumSize) {
                    // If the previous alternative matched without consuming characters then
                    // backtrack to try to match while consumming some input.
                    op.m_zeroLengthMatch = branch32(Equal, index, Address(stackPointerRegister, term->frameLocation * sizeof(void*)));
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
                    ASSERT(endOp->m_op == OpSimpleNestedAlternativeNext || endOp->m_op == OpNestedAlternativeNext);
                    endOp = &m_ops[endOp->m_nextOp];
                }
                ASSERT(endOp->m_op == OpSimpleNestedAlternativeEnd || endOp->m_op == OpNestedAlternativeEnd);
                endOp->m_jumps.append(jump());

                // This is the entry point for the next alternative.
                op.m_reentry = label();

                // Calculate how much input we need to check for, and if non-zero check.
                op.m_checkAdjust = alternative->m_minimumSize;
                if ((term->quantityType == QuantifierFixedCount) && (term->type != PatternTerm::TypeParentheticalAssertion))
                    op.m_checkAdjust -= disjunction->m_minimumSize;
                if (op.m_checkAdjust)
                    op.m_jumps.append(jumpIfNoAvailableInput(op.m_checkAdjust.unsafeGet()));

                YarrOp& lastOp = m_ops[op.m_previousOp];
                m_checkedOffset -= lastOp.m_checkAdjust;
                m_checkedOffset += op.m_checkAdjust;
                break;
            }
            case OpSimpleNestedAlternativeEnd:
            case OpNestedAlternativeEnd: {
                PatternTerm* term = op.m_term;

                // In the non-simple case, store a 'return address' so we can backtrack correctly.
                if (op.m_op == OpNestedAlternativeEnd) {
                    unsigned parenthesesFrameLocation = term->frameLocation;
                    op.m_returnAddress = storeToFrameWithPatch(parenthesesFrameLocation + BackTrackInfoParentheses::returnAddressIndex());
                }

                if (term->quantityType != QuantifierFixedCount && !m_ops[op.m_previousOp].m_alternative->m_minimumSize) {
                    // If the previous alternative matched without consuming characters then
                    // backtrack to try to match while consumming some input.
                    op.m_zeroLengthMatch = branch32(Equal, index, Address(stackPointerRegister, term->frameLocation * sizeof(void*)));
                }

                // If this set of alternatives contains more than one alternative,
                // then the Next nodes will have planted jumps to the End, and added
                // them to this node's m_jumps list.
                op.m_jumps.link(this);
                op.m_jumps.clear();

                YarrOp& lastOp = m_ops[op.m_previousOp];
                m_checkedOffset -= lastOp.m_checkAdjust;
                break;
            }

            // OpParenthesesSubpatternOnceBegin/End
            //
            // These nodes support (optionally) capturing subpatterns, that have a
            // quantity count of 1 (this covers fixed once, and ?/?? quantifiers). 
            case OpParenthesesSubpatternOnceBegin: {
                PatternTerm* term = op.m_term;
                unsigned parenthesesFrameLocation = term->frameLocation;
                const RegisterID indexTemporary = regT0;
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
                if (term->quantityType == QuantifierGreedy)
                    storeToFrame(index, parenthesesFrameLocation + BackTrackInfoParenthesesOnce::beginIndex());
                else if (term->quantityType == QuantifierNonGreedy) {
                    storeToFrame(TrustedImm32(-1), parenthesesFrameLocation + BackTrackInfoParenthesesOnce::beginIndex());
                    op.m_jumps.append(jump());
                    op.m_reentry = label();
                    storeToFrame(index, parenthesesFrameLocation + BackTrackInfoParenthesesOnce::beginIndex());
                }

                // If the parenthese are capturing, store the starting index value to the
                // captures array, offsetting as necessary.
                //
                // FIXME: could avoid offsetting this value in JIT code, apply
                // offsets only afterwards, at the point the results array is
                // being accessed.
                if (term->capture() && compileMode == IncludeSubpatterns) {
                    unsigned inputOffset = (m_checkedOffset - term->inputPosition).unsafeGet();
                    if (term->quantityType == QuantifierFixedCount)
                        inputOffset += term->parentheses.disjunction->m_minimumSize;
                    if (inputOffset) {
                        move(index, indexTemporary);
                        sub32(Imm32(inputOffset), indexTemporary);
                        setSubpatternStart(indexTemporary, term->parentheses.subpatternId);
                    } else
                        setSubpatternStart(index, term->parentheses.subpatternId);
                }
                break;
            }
            case OpParenthesesSubpatternOnceEnd: {
                PatternTerm* term = op.m_term;
                const RegisterID indexTemporary = regT0;
                ASSERT(term->quantityMaxCount == 1);

                // If the nested alternative matched without consuming any characters, punt this back to the interpreter.
                // FIXME: <https://bugs.webkit.org/show_bug.cgi?id=200786> Add ability for the YARR JIT to properly
                // handle nested expressions that can match without consuming characters
                if (term->quantityType != QuantifierFixedCount && !term->parentheses.disjunction->m_minimumSize)
                    m_abortExecution.append(branch32(Equal, index, Address(stackPointerRegister, term->frameLocation * sizeof(void*))));

                // If the parenthese are capturing, store the ending index value to the
                // captures array, offsetting as necessary.
                //
                // FIXME: could avoid offsetting this value in JIT code, apply
                // offsets only afterwards, at the point the results array is
                // being accessed.
                if (term->capture() && compileMode == IncludeSubpatterns) {
                    unsigned inputOffset = (m_checkedOffset - term->inputPosition).unsafeGet();
                    if (inputOffset) {
                        move(index, indexTemporary);
                        sub32(Imm32(inputOffset), indexTemporary);
                        setSubpatternEnd(indexTemporary, term->parentheses.subpatternId);
                    } else
                        setSubpatternEnd(index, term->parentheses.subpatternId);
                }

                // If the parentheses are quantified Greedy then add a label to jump back
                // to if we get a failed match from after the parentheses. For NonGreedy
                // parentheses, link the jump from before the subpattern to here.
                if (term->quantityType == QuantifierGreedy)
                    op.m_reentry = label();
                else if (term->quantityType == QuantifierNonGreedy) {
                    YarrOp& beginOp = m_ops[op.m_previousOp];
                    beginOp.m_jumps.link(this);
                }
                break;
            }

            // OpParenthesesSubpatternTerminalBegin/End
            case OpParenthesesSubpatternTerminalBegin: {
                PatternTerm* term = op.m_term;
                ASSERT(term->quantityType == QuantifierGreedy);
                ASSERT(term->quantityMaxCount == quantifyInfinite);
                ASSERT(!term->capture());

                // Upon entry set a label to loop back to.
                op.m_reentry = label();

                // Store the start index of the current match; we need to reject zero
                // length matches.
                storeToFrame(index, term->frameLocation + BackTrackInfoParenthesesTerminal::beginIndex());
                break;
            }
            case OpParenthesesSubpatternTerminalEnd: {
                YarrOp& beginOp = m_ops[op.m_previousOp];
                PatternTerm* term = op.m_term;

                // If the nested alternative matched without consuming any characters, punt this back to the interpreter.
                // FIXME: <https://bugs.webkit.org/show_bug.cgi?id=200786> Add ability for the YARR JIT to properly
                // handle nested expressions that can match without consuming characters
                if (term->quantityType != QuantifierFixedCount && !term->parentheses.disjunction->m_minimumSize)
                    m_abortExecution.append(branch32(Equal, index, Address(stackPointerRegister, term->frameLocation * sizeof(void*))));

                // We know that the match is non-zero, we can accept it and
                // loop back up to the head of the subpattern.
                jump(beginOp.m_reentry);

                // This is the entry point to jump to when we stop matching - we will
                // do so once the subpattern cannot match any more.
                op.m_reentry = label();
                break;
            }

            // OpParenthesesSubpatternBegin/End
            //
            // These nodes support generic subpatterns.
            case OpParenthesesSubpatternBegin: {
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
                if (term->quantityType == QuantifierGreedy || term->quantityType == QuantifierNonGreedy) {
                    storeToFrame(TrustedImm32(0), parenthesesFrameLocation + BackTrackInfoParentheses::matchAmountIndex());
                    storeToFrame(TrustedImmPtr(nullptr), parenthesesFrameLocation + BackTrackInfoParentheses::parenContextHeadIndex());

                    if (term->quantityType == QuantifierNonGreedy) {
                        storeToFrame(TrustedImm32(-1), parenthesesFrameLocation + BackTrackInfoParentheses::beginIndex());
                        op.m_jumps.append(jump());
                    }
                    
                    op.m_reentry = label();
                    RegisterID currParenContextReg = regT0;
                    RegisterID newParenContextReg = regT1;

                    loadFromFrame(parenthesesFrameLocation + BackTrackInfoParentheses::parenContextHeadIndex(), currParenContextReg);
                    allocateParenContext(newParenContextReg);
                    storePtr(currParenContextReg, newParenContextReg);
                    storeToFrame(newParenContextReg, parenthesesFrameLocation + BackTrackInfoParentheses::parenContextHeadIndex());
                    saveParenContext(newParenContextReg, regT2, term->parentheses.subpatternId, term->parentheses.lastSubpatternId, parenthesesFrameLocation);
                    storeToFrame(index, parenthesesFrameLocation + BackTrackInfoParentheses::beginIndex());
                }

                // If the parenthese are capturing, store the starting index value to the
                // captures array, offsetting as necessary.
                //
                // FIXME: could avoid offsetting this value in JIT code, apply
                // offsets only afterwards, at the point the results array is
                // being accessed.
                if (term->capture() && compileMode == IncludeSubpatterns) {
                    const RegisterID indexTemporary = regT0;
                    unsigned inputOffset = (m_checkedOffset - term->inputPosition).unsafeGet();
                    if (term->quantityType == QuantifierFixedCount)
                        inputOffset += term->parentheses.disjunction->m_minimumSize;
                    if (inputOffset) {
                        move(index, indexTemporary);
                        sub32(Imm32(inputOffset), indexTemporary);
                        setSubpatternStart(indexTemporary, term->parentheses.subpatternId);
                    } else
                        setSubpatternStart(index, term->parentheses.subpatternId);
                }
#else // !YARR_JIT_ALL_PARENS_EXPRESSIONS
                RELEASE_ASSERT_NOT_REACHED();
#endif
                break;
            }
            case OpParenthesesSubpatternEnd: {
#if ENABLE(YARR_JIT_ALL_PARENS_EXPRESSIONS)
                PatternTerm* term = op.m_term;
                unsigned parenthesesFrameLocation = term->frameLocation;

                // If the nested alternative matched without consuming any characters, punt this back to the interpreter.
                // FIXME: <https://bugs.webkit.org/show_bug.cgi?id=200786> Add ability for the YARR JIT to properly
                // handle nested expressions that can match without consuming characters
                if (term->quantityType != QuantifierFixedCount && !term->parentheses.disjunction->m_minimumSize)
                    m_abortExecution.append(branch32(Equal, index, Address(stackPointerRegister, parenthesesFrameLocation * sizeof(void*))));

                const RegisterID countTemporary = regT1;

                YarrOp& beginOp = m_ops[op.m_previousOp];
                loadFromFrame(parenthesesFrameLocation + BackTrackInfoParentheses::matchAmountIndex(), countTemporary);
                add32(TrustedImm32(1), countTemporary);
                storeToFrame(countTemporary, parenthesesFrameLocation + BackTrackInfoParentheses::matchAmountIndex());

                // If the parenthese are capturing, store the ending index value to the
                // captures array, offsetting as necessary.
                //
                // FIXME: could avoid offsetting this value in JIT code, apply
                // offsets only afterwards, at the point the results array is
                // being accessed.
                if (term->capture() && compileMode == IncludeSubpatterns) {
                    const RegisterID indexTemporary = regT0;
                    
                    unsigned inputOffset = (m_checkedOffset - term->inputPosition).unsafeGet();
                    if (inputOffset) {
                        move(index, indexTemporary);
                        sub32(Imm32(inputOffset), indexTemporary);
                        setSubpatternEnd(indexTemporary, term->parentheses.subpatternId);
                    } else
                        setSubpatternEnd(index, term->parentheses.subpatternId);
                }

                // If the parentheses are quantified Greedy then add a label to jump back
                // to if we get a failed match from after the parentheses. For NonGreedy
                // parentheses, link the jump from before the subpattern to here.
                if (term->quantityType == QuantifierGreedy) {
                    if (term->quantityMaxCount != quantifyInfinite)
                        branch32(Below, countTemporary, Imm32(term->quantityMaxCount.unsafeGet())).linkTo(beginOp.m_reentry, this);
                    else
                        jump(beginOp.m_reentry);
                    
                    op.m_reentry = label();
                } else if (term->quantityType == QuantifierNonGreedy) {
                    YarrOp& beginOp = m_ops[op.m_previousOp];
                    beginOp.m_jumps.link(this);
                    op.m_reentry = label();
                }
#else // !YARR_JIT_ALL_PARENS_EXPRESSIONS
                RELEASE_ASSERT_NOT_REACHED();
#endif
                break;
            }

            // OpParentheticalAssertionBegin/End
            case OpParentheticalAssertionBegin: {
                PatternTerm* term = op.m_term;

                // Store the current index - assertions should not update index, so
                // we will need to restore it upon a successful match.
                unsigned parenthesesFrameLocation = term->frameLocation;
                storeToFrame(index, parenthesesFrameLocation + BackTrackInfoParentheticalAssertion::beginIndex());

                // Check 
                op.m_checkAdjust = m_checkedOffset - term->inputPosition;
                if (op.m_checkAdjust)
                    sub32(Imm32(op.m_checkAdjust.unsafeGet()), index);

                m_checkedOffset -= op.m_checkAdjust;
                break;
            }
            case OpParentheticalAssertionEnd: {
                PatternTerm* term = op.m_term;

                // Restore the input index value.
                unsigned parenthesesFrameLocation = term->frameLocation;
                loadFromFrame(parenthesesFrameLocation + BackTrackInfoParentheticalAssertion::beginIndex(), index);

                // If inverted, a successful match of the assertion must be treated
                // as a failure, so jump to backtracking.
                if (term->invert()) {
                    op.m_jumps.append(jump());
                    op.m_reentry = label();
                }

                YarrOp& lastOp = m_ops[op.m_previousOp];
                m_checkedOffset += lastOp.m_checkAdjust;
                break;
            }

            case OpMatchFailed:
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
                m_disassembler->setForBacktrack(opIndex, label());

            YarrOp& op = m_ops[opIndex];
            switch (op.m_op) {

            case OpTerm:
                backtrackTerm(opIndex);
                break;

            // OpBodyAlternativeBegin/Next/End
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
            case OpBodyAlternativeBegin:
            case OpBodyAlternativeNext: {
                PatternAlternative* alternative = op.m_alternative;

                m_checkedOffset -= alternative->m_minimumSize;
                if (op.m_op == OpBodyAlternativeNext) {
                    PatternAlternative* priorAlternative = m_ops[op.m_previousOp].m_alternative;
                    m_checkedOffset += priorAlternative->m_minimumSize;
                }

                // Is this the last alternative? If not, then if we backtrack to this point we just
                // need to jump to try to match the next alternative.
                if (m_ops[op.m_nextOp].m_op != OpBodyAlternativeEnd) {
                    m_backtrackingState.linkTo(m_ops[op.m_nextOp].m_reentry, this);
                    break;
                }
                YarrOp& endOp = m_ops[op.m_nextOp];

                YarrOp* beginOp = &op;
                while (beginOp->m_op != OpBodyAlternativeBegin) {
                    ASSERT(beginOp->m_op == OpBodyAlternativeNext);
                    beginOp = &m_ops[beginOp->m_previousOp];
                }

                bool onceThrough = endOp.m_nextOp == notFound;
                
                JumpList lastStickyAlternativeFailures;

                // First, generate code to handle cases where we backtrack out of an attempted match
                // of the last alternative. If this is a 'once through' set of alternatives then we
                // have nothing to do - link this straight through to the End.
                if (onceThrough)
                    m_backtrackingState.linkTo(endOp.m_reentry, this);
                else {
                    if (m_pattern.sticky() && m_ops[op.m_nextOp].m_op == OpBodyAlternativeEnd) {
                        // It is a sticky pattern and the last alternative failed, jump to the end.
                        m_backtrackingState.takeBacktracksToJumpList(lastStickyAlternativeFailures, this);
                    } else if (m_pattern.m_body->m_hasFixedSize
                        && (alternative->m_minimumSize > beginOp->m_alternative->m_minimumSize)
                        && (alternative->m_minimumSize - beginOp->m_alternative->m_minimumSize == 1)) {
                        // If we don't need to move the input position, and the pattern has a fixed size
                        // (in which case we omit the store of the start index until the pattern has matched)
                        // then we can just link the backtrack out of the last alternative straight to the
                        // head of the first alternative.
                        m_backtrackingState.linkTo(beginOp->m_reentry, this);
                    } else {
                        // We need to generate a trampoline of code to execute before looping back
                        // around to the first alternative.
                        m_backtrackingState.link(this);

                        // No need to advance and retry for a sticky pattern.
                        if (!m_pattern.sticky()) {
                            // If the pattern size is not fixed, then store the start index for use if we match.
                            if (!m_pattern.m_body->m_hasFixedSize) {
                                if (alternative->m_minimumSize == 1)
                                    setMatchStart(index);
                                else {
                                    move(index, regT0);
                                    if (alternative->m_minimumSize)
                                        sub32(Imm32(alternative->m_minimumSize - 1), regT0);
                                    else
                                        add32(TrustedImm32(1), regT0);
                                    setMatchStart(regT0);
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
                                    sub32(Imm32(delta - 1), index);
                                jump(beginOp->m_reentry);
                            } else {
                                // If the first alternative has minimum size 0xFFFFFFFFu, then there cannot
                                // be sufficent input available to handle this, so just fall through.
                                unsigned delta = beginOp->m_alternative->m_minimumSize - alternative->m_minimumSize;
                                if (delta != 0xFFFFFFFFu) {
                                    // We need to check input because we are incrementing the input.
                                    add32(Imm32(delta + 1), index);
                                    checkInput().linkTo(beginOp->m_reentry, this);
                                }
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
                Label firstInputCheckFailed(this);

                // Generate code to handle input check failures from alternatives except the last.
                // prevOp is the alternative we're handling a bail out from (initially Begin), and
                // nextOp is the alternative we will be attempting to reenter into.
                // 
                // We will link input check failures from the forwards matching path back to the code
                // that can handle them.
                YarrOp* prevOp = beginOp;
                YarrOp* nextOp = &m_ops[beginOp->m_nextOp];
                while (nextOp->m_op != OpBodyAlternativeEnd) {
                    prevOp->m_jumps.link(this);

                    // We only get here if an input check fails, it is only worth checking again
                    // if the next alternative has a minimum size less than the last.
                    if (prevOp->m_alternative->m_minimumSize > nextOp->m_alternative->m_minimumSize) {
                        // FIXME: if we added an extra label to YarrOp, we could avoid needing to
                        // subtract delta back out, and reduce this code. Should performance test
                        // the benefit of this.
                        unsigned delta = prevOp->m_alternative->m_minimumSize - nextOp->m_alternative->m_minimumSize;
                        sub32(Imm32(delta), index);
                        Jump fail = jumpIfNoAvailableInput();
                        add32(Imm32(delta), index);
                        jump(nextOp->m_reentry);
                        fail.link(this);
                    } else if (prevOp->m_alternative->m_minimumSize < nextOp->m_alternative->m_minimumSize)
                        add32(Imm32(nextOp->m_alternative->m_minimumSize - prevOp->m_alternative->m_minimumSize), index);
                    prevOp = nextOp;
                    nextOp = &m_ops[nextOp->m_nextOp];
                }

                // We fall through to here if there is insufficient input to run the last alternative.

                // If there is insufficient input to run the last alternative, then for 'once through'
                // alternatives we are done - just jump back up into the forwards matching path at the End.
                if (onceThrough) {
                    op.m_jumps.linkTo(endOp.m_reentry, this);
                    jump(endOp.m_reentry);
                    break;
                }

                // For repeating alternatives, link any input check failure from the last alternative to
                // this point.
                op.m_jumps.link(this);

                bool needsToUpdateMatchStart = !m_pattern.m_body->m_hasFixedSize;

                // Check for cases where input position is already incremented by 1 for the last
                // alternative (this is particularly useful where the minimum size of the body
                // disjunction is 0, e.g. /a*|b/).
                if (needsToUpdateMatchStart && alternative->m_minimumSize == 1) {
                    // index is already incremented by 1, so just store it now!
                    setMatchStart(index);
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
                        add32(TrustedImm32(1), index);
                    } else {
                        // If the minumum for the last alternative was one greater than than that
                        // for the disjunction, we're already progressed by 1, nothing to do!
                        unsigned delta = (alternative->m_minimumSize - m_pattern.m_body->m_minimumSize) - 1;
                        if (delta)
                            sub32(Imm32(delta), index);
                    }
                    Jump matchFailed = jumpIfNoAvailableInput();

                    if (needsToUpdateMatchStart) {
                        if (!m_pattern.m_body->m_minimumSize)
                            setMatchStart(index);
                        else {
                            move(index, regT0);
                            sub32(Imm32(m_pattern.m_body->m_minimumSize), regT0);
                            setMatchStart(regT0);
                        }
                    }

                    // Calculate how much more input the first alternative requires than the minimum
                    // for the body as a whole. If no more is needed then we dont need an additional
                    // input check here - jump straight back up to the start of the first alternative.
                    if (beginOp->m_alternative->m_minimumSize == m_pattern.m_body->m_minimumSize)
                        jump(beginOp->m_reentry);
                    else {
                        if (beginOp->m_alternative->m_minimumSize > m_pattern.m_body->m_minimumSize)
                            add32(Imm32(beginOp->m_alternative->m_minimumSize - m_pattern.m_body->m_minimumSize), index);
                        else
                            sub32(Imm32(m_pattern.m_body->m_minimumSize - beginOp->m_alternative->m_minimumSize), index);
                        checkInput().linkTo(beginOp->m_reentry, this);
                        jump(firstInputCheckFailed);
                    }

                    // We jump to here if we iterate to the point that there is insufficient input to
                    // run any matches, and need to return a failure state from JIT code.
                    matchFailed.link(this);
                }

                lastStickyAlternativeFailures.link(this);
                removeCallFrame();
                generateFailReturn();
                break;
            }
            case OpBodyAlternativeEnd: {
                // We should never backtrack back into a body disjunction.
                ASSERT(m_backtrackingState.isEmpty());

                PatternAlternative* priorAlternative = m_ops[op.m_previousOp].m_alternative;
                m_checkedOffset += priorAlternative->m_minimumSize;
                break;
            }

            // OpSimpleNestedAlternativeBegin/Next/End
            // OpNestedAlternativeBegin/Next/End
            //
            // Generate code for when we backtrack back out of an alternative into
            // a Begin or Next node, or when the entry input count check fails. If
            // there are more alternatives we need to jump to the next alternative,
            // if not we backtrack back out of the current set of parentheses.
            //
            // In the case of non-simple nested assertions we need to also link the
            // 'return address' appropriately to backtrack back out into the correct
            // alternative.
            case OpSimpleNestedAlternativeBegin:
            case OpSimpleNestedAlternativeNext:
            case OpNestedAlternativeBegin:
            case OpNestedAlternativeNext: {
                YarrOp& nextOp = m_ops[op.m_nextOp];
                bool isBegin = op.m_previousOp == notFound;
                bool isLastAlternative = nextOp.m_nextOp == notFound;
                ASSERT(isBegin == (op.m_op == OpSimpleNestedAlternativeBegin || op.m_op == OpNestedAlternativeBegin));
                ASSERT(isLastAlternative == (nextOp.m_op == OpSimpleNestedAlternativeEnd || nextOp.m_op == OpNestedAlternativeEnd));

                // Treat an input check failure the same as a failed match.
                m_backtrackingState.append(op.m_jumps);

                // Set the backtracks to jump to the appropriate place. We may need
                // to link the backtracks in one of three different way depending on
                // the type of alternative we are dealing with:
                //  - A single alternative, with no simplings.
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
                    m_backtrackingState.link(this);
                    sub32(Imm32(op.m_checkAdjust.unsafeGet()), index);
                    if (!isLastAlternative) {
                        // An alternative that is not the last should jump to its successor.
                        jump(nextOp.m_reentry);
                    } else if (!isBegin) {
                        // The last of more than one alternatives must jump back to the beginning.
                        nextOp.m_jumps.append(jump());
                    } else {
                        // A single alternative on its own can fall through.
                        m_backtrackingState.fallthrough();
                    }
                } else {
                    // Handle the cases where we can link the backtracks directly to their destinations.
                    if (!isLastAlternative) {
                        // An alternative that is not the last should jump to its successor.
                        m_backtrackingState.linkTo(nextOp.m_reentry, this);
                    } else if (!isBegin) {
                        // The last of more than one alternatives must jump back to the beginning.
                        m_backtrackingState.takeBacktracksToJumpList(nextOp.m_jumps, this);
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
                if (op.m_op == OpNestedAlternativeNext)
                    m_backtrackingState.append(op.m_returnAddress);

                // If there is more than one alternative, then the last alternative will
                // have planted a jump to be linked to the end. This jump was added to the
                // End node's m_jumps list. If we are back at the beginning, link it here.
                if (isBegin) {
                    YarrOp* endOp = &m_ops[op.m_nextOp];
                    while (endOp->m_nextOp != notFound) {
                        ASSERT(endOp->m_op == OpSimpleNestedAlternativeNext || endOp->m_op == OpNestedAlternativeNext);
                        endOp = &m_ops[endOp->m_nextOp];
                    }
                    ASSERT(endOp->m_op == OpSimpleNestedAlternativeEnd || endOp->m_op == OpNestedAlternativeEnd);
                    m_backtrackingState.append(endOp->m_jumps);
                }

                m_checkedOffset -= op.m_checkAdjust;
                if (!isBegin) {
                    YarrOp& lastOp = m_ops[op.m_previousOp];
                    m_checkedOffset += lastOp.m_checkAdjust;
                }
                break;
            }
            case OpSimpleNestedAlternativeEnd:
            case OpNestedAlternativeEnd: {
                PatternTerm* term = op.m_term;

                // If there is a backtrack jump from a zero length match link it here.
                if (op.m_zeroLengthMatch.isSet())
                    m_backtrackingState.append(op.m_zeroLengthMatch);

                // If we backtrack into the end of a simple subpattern do nothing;
                // just continue through into the last alternative. If we backtrack
                // into the end of a non-simple set of alterntives we need to jump
                // to the backtracking return address set up during generation.
                if (op.m_op == OpNestedAlternativeEnd) {
                    m_backtrackingState.link(this);

                    // Plant a jump to the return address.
                    unsigned parenthesesFrameLocation = term->frameLocation;
                    loadFromFrameAndJump(parenthesesFrameLocation + BackTrackInfoParentheses::returnAddressIndex());

                    // Link the DataLabelPtr associated with the end of the last
                    // alternative to this point.
                    m_backtrackingState.append(op.m_returnAddress);
                }

                YarrOp& lastOp = m_ops[op.m_previousOp];
                m_checkedOffset += lastOp.m_checkAdjust;
                break;
            }

            // OpParenthesesSubpatternOnceBegin/End
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
            case OpParenthesesSubpatternOnceBegin: {
                PatternTerm* term = op.m_term;
                ASSERT(term->quantityMaxCount == 1);

                // We only need to backtrack to this point if capturing or greedy.
                if ((term->capture() && compileMode == IncludeSubpatterns) || term->quantityType == QuantifierGreedy) {
                    m_backtrackingState.link(this);

                    // If capturing, clear the capture (we only need to reset start).
                    if (term->capture() && compileMode == IncludeSubpatterns)
                        clearSubpatternStart(term->parentheses.subpatternId);

                    // If Greedy, jump to the end.
                    if (term->quantityType == QuantifierGreedy) {
                        // Clear the flag in the stackframe indicating we ran through the subpattern.
                        unsigned parenthesesFrameLocation = term->frameLocation;
                        storeToFrame(TrustedImm32(-1), parenthesesFrameLocation + BackTrackInfoParenthesesOnce::beginIndex());
                        // Jump to after the parentheses, skipping the subpattern.
                        jump(m_ops[op.m_nextOp].m_reentry);
                        // A backtrack from after the parentheses, when skipping the subpattern,
                        // will jump back to here.
                        op.m_jumps.link(this);
                    }

                    m_backtrackingState.fallthrough();
                }
                break;
            }
            case OpParenthesesSubpatternOnceEnd: {
                PatternTerm* term = op.m_term;

                if (term->quantityType != QuantifierFixedCount) {
                    m_backtrackingState.link(this);

                    // Check whether we should backtrack back into the parentheses, or if we
                    // are currently in a state where we had skipped over the subpattern
                    // (in which case the flag value on the stack will be -1).
                    unsigned parenthesesFrameLocation = term->frameLocation;
                    Jump hadSkipped = branch32(Equal, Address(stackPointerRegister, (parenthesesFrameLocation + BackTrackInfoParenthesesOnce::beginIndex()) * sizeof(void*)), TrustedImm32(-1));

                    if (term->quantityType == QuantifierGreedy) {
                        // For Greedy parentheses, we skip after having already tried going
                        // through the subpattern, so if we get here we're done.
                        YarrOp& beginOp = m_ops[op.m_previousOp];
                        beginOp.m_jumps.append(hadSkipped);
                    } else {
                        // For NonGreedy parentheses, we try skipping the subpattern first,
                        // so if we get here we need to try running through the subpattern
                        // next. Jump back to the start of the parentheses in the forwards
                        // matching path.
                        ASSERT(term->quantityType == QuantifierNonGreedy);
                        YarrOp& beginOp = m_ops[op.m_previousOp];
                        hadSkipped.linkTo(beginOp.m_reentry, this);
                    }

                    m_backtrackingState.fallthrough();
                }

                m_backtrackingState.append(op.m_jumps);
                break;
            }

            // OpParenthesesSubpatternTerminalBegin/End
            //
            // Terminal subpatterns will always match - there is nothing after them to
            // force a backtrack, and they have a minimum count of 0, and as such will
            // always produce an acceptable result.
            case OpParenthesesSubpatternTerminalBegin: {
                // We will backtrack to this point once the subpattern cannot match any
                // more. Since no match is accepted as a successful match (we are Greedy
                // quantified with a minimum of zero) jump back to the forwards matching
                // path at the end.
                YarrOp& endOp = m_ops[op.m_nextOp];
                m_backtrackingState.linkTo(endOp.m_reentry, this);
                break;
            }
            case OpParenthesesSubpatternTerminalEnd:
                // We should never be backtracking to here (hence the 'terminal' in the name).
                ASSERT(m_backtrackingState.isEmpty());
                m_backtrackingState.append(op.m_jumps);
                break;

            // OpParenthesesSubpatternBegin/End
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
            case OpParenthesesSubpatternBegin: {
#if ENABLE(YARR_JIT_ALL_PARENS_EXPRESSIONS)
                PatternTerm* term = op.m_term;
                unsigned parenthesesFrameLocation = term->frameLocation;

                if (term->quantityType != QuantifierFixedCount) {
                    m_backtrackingState.link(this);

                    RegisterID currParenContextReg = regT0;
                    RegisterID newParenContextReg = regT1;

                    loadFromFrame(parenthesesFrameLocation + BackTrackInfoParentheses::parenContextHeadIndex(), currParenContextReg);
                    
                    restoreParenContext(currParenContextReg, regT2, term->parentheses.subpatternId, term->parentheses.lastSubpatternId, parenthesesFrameLocation);
                    
                    freeParenContext(currParenContextReg, newParenContextReg);
                    storeToFrame(newParenContextReg, parenthesesFrameLocation + BackTrackInfoParentheses::parenContextHeadIndex());

                    const RegisterID countTemporary = regT0;
                    loadFromFrame(parenthesesFrameLocation + BackTrackInfoParentheses::matchAmountIndex(), countTemporary);
                    Jump zeroLengthMatch = branchTest32(Zero, countTemporary);

                    sub32(TrustedImm32(1), countTemporary);
                    storeToFrame(countTemporary, parenthesesFrameLocation + BackTrackInfoParentheses::matchAmountIndex());

                    jump(m_ops[op.m_nextOp].m_reentry);

                    zeroLengthMatch.link(this);

                    // Clear the flag in the stackframe indicating we didn't run through the subpattern.
                    storeToFrame(TrustedImm32(-1), parenthesesFrameLocation + BackTrackInfoParentheses::beginIndex());

                    if (term->quantityType == QuantifierGreedy)
                        jump(m_ops[op.m_nextOp].m_reentry);

                    // If Greedy, jump to the end.
                    if (term->quantityType == QuantifierGreedy) {
                        // A backtrack from after the parentheses, when skipping the subpattern,
                        // will jump back to here.
                        op.m_jumps.link(this);
                    }

                    m_backtrackingState.fallthrough();
                }
#else // !YARR_JIT_ALL_PARENS_EXPRESSIONS
                RELEASE_ASSERT_NOT_REACHED();
#endif
                break;
            }
            case OpParenthesesSubpatternEnd: {
#if ENABLE(YARR_JIT_ALL_PARENS_EXPRESSIONS)
                PatternTerm* term = op.m_term;

                if (term->quantityType != QuantifierFixedCount) {
                    m_backtrackingState.link(this);

                    unsigned parenthesesFrameLocation = term->frameLocation;

                    if (term->quantityType == QuantifierGreedy) {
                        // Check whether we should backtrack back into the parentheses, or if we
                        // are currently in a state where we had skipped over the subpattern
                        // (in which case the flag value on the stack will be -1).
                        Jump hadSkipped = branch32(Equal, Address(stackPointerRegister, (parenthesesFrameLocation  + BackTrackInfoParentheses::beginIndex()) * sizeof(void*)), TrustedImm32(-1));

                        // For Greedy parentheses, we skip after having already tried going
                        // through the subpattern, so if we get here we're done.
                        YarrOp& beginOp = m_ops[op.m_previousOp];
                        beginOp.m_jumps.append(hadSkipped);
                    } else {
                        // For NonGreedy parentheses, we try skipping the subpattern first,
                        // so if we get here we need to try running through the subpattern
                        // next. Jump back to the start of the parentheses in the forwards
                        // matching path.
                        ASSERT(term->quantityType == QuantifierNonGreedy);

                        const RegisterID beginTemporary = regT0;
                        const RegisterID countTemporary = regT1;

                        YarrOp& beginOp = m_ops[op.m_previousOp];

                        loadFromFrame(parenthesesFrameLocation + BackTrackInfoParentheses::beginIndex(), beginTemporary);
                        branch32(Equal, beginTemporary, TrustedImm32(-1)).linkTo(beginOp.m_reentry, this);

                        JumpList exceededMatchLimit;

                        if (term->quantityMaxCount != quantifyInfinite) {
                            loadFromFrame(parenthesesFrameLocation + BackTrackInfoParentheses::matchAmountIndex(), countTemporary);
                            exceededMatchLimit.append(branch32(AboveOrEqual, countTemporary, Imm32(term->quantityMaxCount.unsafeGet())));
                        }

                        branch32(Above, index, beginTemporary).linkTo(beginOp.m_reentry, this);

                        exceededMatchLimit.link(this);
                    }

                    m_backtrackingState.fallthrough();
                }

                m_backtrackingState.append(op.m_jumps);
#else // !YARR_JIT_ALL_PARENS_EXPRESSIONS
                RELEASE_ASSERT_NOT_REACHED();
#endif
                break;
            }

            // OpParentheticalAssertionBegin/End
            case OpParentheticalAssertionBegin: {
                PatternTerm* term = op.m_term;
                YarrOp& endOp = m_ops[op.m_nextOp];

                // We need to handle the backtracks upon backtracking back out
                // of a parenthetical assertion if either we need to correct
                // the input index, or the assertion was inverted.
                if (op.m_checkAdjust || term->invert()) {
                     m_backtrackingState.link(this);

                    if (op.m_checkAdjust)
                        add32(Imm32(op.m_checkAdjust.unsafeGet()), index);

                    // In an inverted assertion failure to match the subpattern
                    // is treated as a successful match - jump to the end of the
                    // subpattern. We already have adjusted the input position
                    // back to that before the assertion, which is correct.
                    if (term->invert())
                        jump(endOp.m_reentry);

                    m_backtrackingState.fallthrough();
                }

                // The End node's jump list will contain any backtracks into
                // the end of the assertion. Also, if inverted, we will have
                // added the failure caused by a successful match to this.
                m_backtrackingState.append(endOp.m_jumps);

                m_checkedOffset += op.m_checkAdjust;
                break;
            }
            case OpParentheticalAssertionEnd: {
                // FIXME: We should really be clearing any nested subpattern
                // matches on bailing out from after the pattern. Firefox has
                // this bug too (presumably because they use YARR!)

                // Never backtrack into an assertion; later failures bail to before the begin.
                m_backtrackingState.takeBacktracksToJumpList(op.m_jumps, this);

                YarrOp& lastOp = m_ops[op.m_previousOp];
                m_checkedOffset -= lastOp.m_checkAdjust;
                break;
            }

            case OpMatchFailed:
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
    void opCompileParenthesesSubpattern(PatternTerm* term)
    {
        YarrOpCode parenthesesBeginOpCode;
        YarrOpCode parenthesesEndOpCode;
        YarrOpCode alternativeBeginOpCode = OpSimpleNestedAlternativeBegin;
        YarrOpCode alternativeNextOpCode = OpSimpleNestedAlternativeNext;
        YarrOpCode alternativeEndOpCode = OpSimpleNestedAlternativeEnd;

        if (UNLIKELY(!m_vm->isSafeToRecurse())) {
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
            parenthesesBeginOpCode = OpParenthesesSubpatternOnceBegin;
            parenthesesEndOpCode = OpParenthesesSubpatternOnceEnd;

            // If there is more than one alternative we cannot use the 'simple' nodes.
            if (term->parentheses.disjunction->m_alternatives.size() != 1) {
                alternativeBeginOpCode = OpNestedAlternativeBegin;
                alternativeNextOpCode = OpNestedAlternativeNext;
                alternativeEndOpCode = OpNestedAlternativeEnd;
            }
        } else if (term->parentheses.isTerminal) {
            // Select the 'Terminal' nodes.
            parenthesesBeginOpCode = OpParenthesesSubpatternTerminalBegin;
            parenthesesEndOpCode = OpParenthesesSubpatternTerminalEnd;
        } else {
#if ENABLE(YARR_JIT_ALL_PARENS_EXPRESSIONS)
            // We only handle generic parenthesis with non-fixed counts.
            if (term->quantityType == QuantifierFixedCount) {
                // This subpattern is not supported by the JIT.
                m_failureReason = JITFailureReason::FixedCountParenthesizedSubpattern;
                return;
            }

            m_containsNestedSubpatterns = true;

            // Select the 'Generic' nodes.
            parenthesesBeginOpCode = OpParenthesesSubpatternBegin;
            parenthesesEndOpCode = OpParenthesesSubpatternEnd;

            // If there is more than one alternative we cannot use the 'simple' nodes.
            if (term->parentheses.disjunction->m_alternatives.size() != 1) {
                alternativeBeginOpCode = OpNestedAlternativeBegin;
                alternativeNextOpCode = OpNestedAlternativeNext;
                alternativeEndOpCode = OpNestedAlternativeEnd;
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
        Vector<std::unique_ptr<PatternAlternative>>& alternatives = term->parentheses.disjunction->m_alternatives;
        for (unsigned i = 0; i < alternatives.size(); ++i) {
            size_t lastOpIndex = m_ops.size() - 1;

            PatternAlternative* nestedAlternative = alternatives[i].get();
            opCompileAlternative(nestedAlternative);

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

        size_t parenEnd = m_ops.size();
        m_ops.append(parenthesesEndOpCode);

        m_ops[parenBegin].m_term = term;
        m_ops[parenBegin].m_previousOp = notFound;
        m_ops[parenBegin].m_nextOp = parenEnd;
        m_ops[parenEnd].m_term = term;
        m_ops[parenEnd].m_previousOp = parenBegin;
        m_ops[parenEnd].m_nextOp = notFound;
    }

    // opCompileParentheticalAssertion
    // Emits ops for a parenthetical assertion. These consist of an
    // OpSimpleNestedAlternativeBegin/Next/End set of nodes wrapping
    // the alternatives, with these wrapped by an outer pair of
    // OpParentheticalAssertionBegin/End nodes.
    // We can always use the OpSimpleNestedAlternative nodes in the
    // case of parenthetical assertions since these only ever match
    // once, and will never backtrack back into the assertion.
    void opCompileParentheticalAssertion(PatternTerm* term)
    {
        if (UNLIKELY(!m_vm->isSafeToRecurse())) {
            m_failureReason = JITFailureReason::ParenthesisNestedTooDeep;
            return;
        }

        size_t parenBegin = m_ops.size();
        m_ops.append(OpParentheticalAssertionBegin);

        m_ops.append(OpSimpleNestedAlternativeBegin);
        m_ops.last().m_previousOp = notFound;
        m_ops.last().m_term = term;
        Vector<std::unique_ptr<PatternAlternative>>& alternatives =  term->parentheses.disjunction->m_alternatives;
        for (unsigned i = 0; i < alternatives.size(); ++i) {
            size_t lastOpIndex = m_ops.size() - 1;

            PatternAlternative* nestedAlternative = alternatives[i].get();
            opCompileAlternative(nestedAlternative);

            size_t thisOpIndex = m_ops.size();
            m_ops.append(YarrOp(OpSimpleNestedAlternativeNext));

            YarrOp& lastOp = m_ops[lastOpIndex];
            YarrOp& thisOp = m_ops[thisOpIndex];

            lastOp.m_alternative = nestedAlternative;
            lastOp.m_nextOp = thisOpIndex;
            thisOp.m_previousOp = lastOpIndex;
            thisOp.m_term = term;
        }
        YarrOp& lastOp = m_ops.last();
        ASSERT(lastOp.m_op == OpSimpleNestedAlternativeNext);
        lastOp.m_op = OpSimpleNestedAlternativeEnd;
        lastOp.m_alternative = nullptr;
        lastOp.m_nextOp = notFound;

        size_t parenEnd = m_ops.size();
        m_ops.append(OpParentheticalAssertionEnd);

        m_ops[parenBegin].m_term = term;
        m_ops[parenBegin].m_previousOp = notFound;
        m_ops[parenBegin].m_nextOp = parenEnd;
        m_ops[parenEnd].m_term = term;
        m_ops[parenEnd].m_previousOp = parenBegin;
        m_ops[parenEnd].m_nextOp = notFound;
    }

    // opCompileAlternative
    // Called to emit nodes for all terms in an alternative.
    void opCompileAlternative(PatternAlternative* alternative)
    {
        optimizeAlternative(alternative);

        for (unsigned i = 0; i < alternative->m_terms.size(); ++i) {
            PatternTerm* term = &alternative->m_terms[i];

            switch (term->type) {
            case PatternTerm::TypeParenthesesSubpattern:
                opCompileParenthesesSubpattern(term);
                break;

            case PatternTerm::TypeParentheticalAssertion:
                opCompileParentheticalAssertion(term);
                break;

            default:
                m_ops.append(term);
            }
        }
    }

    // opCompileBody
    // This method compiles the body disjunction of the regular expression.
    // The body consists of two sets of alternatives - zero or more 'once
    // through' (BOL anchored) alternatives, followed by zero or more
    // repeated alternatives.
    // For each of these two sets of alteratives, if not empty they will be
    // wrapped in a set of OpBodyAlternativeBegin/Next/End nodes (with the
    // 'begin' node referencing the first alternative, and 'next' nodes
    // referencing any further alternatives. The begin/next/end nodes are
    // linked together in a doubly linked list. In the case of repeating
    // alternatives, the end node is also linked back to the beginning.
    // If no repeating alternatives exist, then a OpMatchFailed node exists
    // to return the failing result.
    void opCompileBody(PatternDisjunction* disjunction)
    {
        if (UNLIKELY(!m_vm->isSafeToRecurse())) {
            m_failureReason = JITFailureReason::ParenthesisNestedTooDeep;
            return;
        }
        
        Vector<std::unique_ptr<PatternAlternative>>& alternatives = disjunction->m_alternatives;
        size_t currentAlternativeIndex = 0;

        // Emit the 'once through' alternatives.
        if (alternatives.size() && alternatives[0]->onceThrough()) {
            m_ops.append(YarrOp(OpBodyAlternativeBegin));
            m_ops.last().m_previousOp = notFound;

            do {
                size_t lastOpIndex = m_ops.size() - 1;
                PatternAlternative* alternative = alternatives[currentAlternativeIndex].get();
                opCompileAlternative(alternative);

                size_t thisOpIndex = m_ops.size();
                m_ops.append(YarrOp(OpBodyAlternativeNext));

                YarrOp& lastOp = m_ops[lastOpIndex];
                YarrOp& thisOp = m_ops[thisOpIndex];

                lastOp.m_alternative = alternative;
                lastOp.m_nextOp = thisOpIndex;
                thisOp.m_previousOp = lastOpIndex;
                
                ++currentAlternativeIndex;
            } while (currentAlternativeIndex < alternatives.size() && alternatives[currentAlternativeIndex]->onceThrough());

            YarrOp& lastOp = m_ops.last();

            ASSERT(lastOp.m_op == OpBodyAlternativeNext);
            lastOp.m_op = OpBodyAlternativeEnd;
            lastOp.m_alternative = nullptr;
            lastOp.m_nextOp = notFound;
        }

        if (currentAlternativeIndex == alternatives.size()) {
            m_ops.append(YarrOp(OpMatchFailed));
            return;
        }

        // Emit the repeated alternatives.
        size_t repeatLoop = m_ops.size();
        m_ops.append(YarrOp(OpBodyAlternativeBegin));
        m_ops.last().m_previousOp = notFound;
        do {
            size_t lastOpIndex = m_ops.size() - 1;
            PatternAlternative* alternative = alternatives[currentAlternativeIndex].get();
            ASSERT(!alternative->onceThrough());
            opCompileAlternative(alternative);

            size_t thisOpIndex = m_ops.size();
            m_ops.append(YarrOp(OpBodyAlternativeNext));

            YarrOp& lastOp = m_ops[lastOpIndex];
            YarrOp& thisOp = m_ops[thisOpIndex];

            lastOp.m_alternative = alternative;
            lastOp.m_nextOp = thisOpIndex;
            thisOp.m_previousOp = lastOpIndex;
            
            ++currentAlternativeIndex;
        } while (currentAlternativeIndex < alternatives.size());
        YarrOp& lastOp = m_ops.last();
        ASSERT(lastOp.m_op == OpBodyAlternativeNext);
        lastOp.m_op = OpBodyAlternativeEnd;
        lastOp.m_alternative = nullptr;
        lastOp.m_nextOp = repeatLoop;
    }

    void generateTryReadUnicodeCharacterHelper()
    {
#ifdef JIT_UNICODE_EXPRESSIONS
        if (m_tryReadUnicodeCharacterCalls.isEmpty())
            return;

        ASSERT(m_decodeSurrogatePairs);

        m_tryReadUnicodeCharacterEntry = label();

        tagReturnAddress();

        tryReadUnicodeCharImpl(regT0);

        ret();
#endif
    }

    void generateEnter()
    {
#if CPU(X86_64)
        push(X86Registers::ebp);
        move(stackPointerRegister, X86Registers::ebp);

        if (m_pattern.m_saveInitialStartValue)
            push(X86Registers::ebx);

#if OS(WINDOWS)
        push(X86Registers::edi);
#endif
#if ENABLE(YARR_JIT_ALL_PARENS_EXPRESSIONS)
        if (m_containsNestedSubpatterns) {
#if OS(WINDOWS)
            push(X86Registers::esi);
#endif
            push(X86Registers::r12);
        }
#endif

        if (m_decodeSurrogatePairs) {
            push(X86Registers::r13);
            push(X86Registers::r14);
            push(X86Registers::r15);
        }
        // The ABI doesn't guarantee the upper bits are zero on unsigned arguments, so clear them ourselves.
        zeroExtend32ToWord(index, index);
        zeroExtend32ToWord(length, length);
#if OS(WINDOWS)
        if (compileMode == IncludeSubpatterns)
            loadPtr(Address(X86Registers::ebp, 6 * sizeof(void*)), output);
        // rcx is the pointer to the allocated space for result in x64 Windows.
        push(X86Registers::ecx);
#endif
#elif CPU(ARM64)
        if (!Options::useJITCage())
            tagReturnAddress();
        if (m_decodeSurrogatePairs) {
            if (!Options::useJITCage())
                pushPair(framePointerRegister, linkRegister);
            move(TrustedImm32(0x10000), supplementaryPlanesBase);
            move(TrustedImm32(0xd800), leadingSurrogateTag);
            move(TrustedImm32(0xdc00), trailingSurrogateTag);
        }

        // The ABI doesn't guarantee the upper bits are zero on unsigned arguments, so clear them ourselves.
        zeroExtend32ToWord(index, index);
        zeroExtend32ToWord(length, length);
#elif CPU(ARM_THUMB2)
        push(ARMRegisters::r4);
        push(ARMRegisters::r5);
        push(ARMRegisters::r6);
        push(ARMRegisters::r8);
#elif CPU(MIPS)
        // Do nothing.
#endif

        store8(TrustedImm32(1), &m_vm->isExecutingInRegExpJIT);
    }

    void generateReturn()
    {
        store8(TrustedImm32(0), &m_vm->isExecutingInRegExpJIT);

#if CPU(X86_64)
#if OS(WINDOWS)
        // Store the return value in the allocated space pointed by rcx.
        pop(X86Registers::ecx);
        store64(returnRegister, Address(X86Registers::ecx));
        store64(returnRegister2, Address(X86Registers::ecx, sizeof(void*)));
        move(X86Registers::ecx, returnRegister);
#endif
        if (m_decodeSurrogatePairs) {
            pop(X86Registers::r15);
            pop(X86Registers::r14);
            pop(X86Registers::r13);
        }

#if ENABLE(YARR_JIT_ALL_PARENS_EXPRESSIONS)
        if (m_containsNestedSubpatterns) {
            pop(X86Registers::r12);
#if OS(WINDOWS)
            pop(X86Registers::esi);
#endif
        }
#endif
#if OS(WINDOWS)
        pop(X86Registers::edi);
#endif

        if (m_pattern.m_saveInitialStartValue)
            pop(X86Registers::ebx);
        pop(X86Registers::ebp);
#elif CPU(ARM64)
        if (m_decodeSurrogatePairs) {
            if (!Options::useJITCage())
                popPair(framePointerRegister, linkRegister);
        }
#elif CPU(ARM_THUMB2)
        pop(ARMRegisters::r8);
        pop(ARMRegisters::r6);
        pop(ARMRegisters::r5);
        pop(ARMRegisters::r4);
#elif CPU(MIPS)
        // Do nothing
#endif

#if CPU(ARM64E)
        if (Options::useJITCage())
            farJump(TrustedImmPtr(retagCodePtr<void*, CFunctionPtrTag, OperationPtrTag>(&vmEntryToYarrJITAfter)), OperationPtrTag);
        else
            ret();
#else
        ret();
#endif
    }

public:
    YarrGenerator(VM* vm, YarrPattern& pattern, String& patternString, YarrCodeBlock& codeBlock, YarrCharSize charSize)
        : m_vm(vm)
        , m_pattern(pattern)
        , m_patternString(patternString)
        , m_codeBlock(codeBlock)
        , m_charSize(charSize)
        , m_decodeSurrogatePairs(m_charSize == Char16 && m_pattern.unicode())
        , m_unicodeIgnoreCase(m_pattern.unicode() && m_pattern.ignoreCase())
        , m_fixedSizedAlternative(false)
        , m_canonicalMode(m_pattern.unicode() ? CanonicalMode::Unicode : CanonicalMode::UCS2)
#if ENABLE(YARR_JIT_ALL_PARENS_EXPRESSIONS)
        , m_containsNestedSubpatterns(false)
        , m_parenContextSizes(compileMode == IncludeSubpatterns ? m_pattern.m_numSubpatterns : 0, m_pattern.m_body->m_callFrameSize)
#endif
    {
    }

    void compile()
    {
        YarrCodeBlock& codeBlock = m_codeBlock;

#ifndef JIT_UNICODE_EXPRESSIONS
        if (m_decodeSurrogatePairs) {
            codeBlock.setFallBackWithFailureReason(JITFailureReason::DecodeSurrogatePair);
            return;
        }
#endif

        if (m_pattern.m_containsBackreferences
#if ENABLE(YARR_JIT_BACKREFERENCES)
            && (compileMode == MatchOnly || (m_pattern.ignoreCase() && m_charSize != Char8))
#endif
            ) {
                codeBlock.setFallBackWithFailureReason(JITFailureReason::BackReference);
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
            m_disassembler->setStartOfCode(label());

#if ENABLE(YARR_JIT_ALL_PARENS_EXPRESSIONS)
        if (m_containsNestedSubpatterns)
            codeBlock.setUsesPatternContextBuffer();
#endif

        generateEnter();

        Jump hasInput = checkInput();
        generateFailReturn();
        hasInput.link(this);

        unsigned callFrameSizeInBytes = alignCallFrameSizeInBytes(m_pattern.m_body->m_callFrameSize);
        if (callFrameSizeInBytes) {
            // Check stack size
            addPtr(TrustedImm32(-callFrameSizeInBytes), stackPointerRegister, regT0);
#if CPU(X86_64) && OS(WINDOWS)
            // matchingContext is the 5th argument, it is found on the stack.
            RegisterID matchingContext = regT1;
            loadPtr(Address(X86Registers::ebp, 7 * sizeof(void*)), matchingContext);
#elif CPU(ARM_THUMB2) || CPU(MIPS)
            // matchingContext is the 5th argument, it is found on the stack.
            RegisterID matchingContext = regT1;
            loadPtr(Address(stackPointerRegister, 4 * sizeof(void*)), matchingContext);
#endif
            Jump stackOk = branchPtr(BelowOrEqual, Address(matchingContext, MatchingContextHolder::offsetOfStackLimit()), regT0);

            // Exceeded stack limit, punt to the interpreter.
            move(TrustedImmPtr((void*)static_cast<size_t>(JSRegExpJITCodeFailure)), returnRegister);
            move(TrustedImm32(0), returnRegister2);
            generateReturn();

            stackOk.link(this);
            move(regT0, stackPointerRegister);
        }

#ifdef JIT_UNICODE_EXPRESSIONS
        if (m_decodeSurrogatePairs)
            getEffectiveAddress(BaseIndex(input, length, TimesTwo), endOfStringAddress);
#endif

#if ENABLE(YARR_JIT_ALL_PARENS_EXPRESSIONS)
        if (m_containsNestedSubpatterns)
            move(TrustedImm32(matchLimit), remainingMatchCount);
#endif

        if (compileMode == IncludeSubpatterns) {
            for (unsigned i = 0; i < m_pattern.m_numSubpatterns + 1; ++i)
                store32(TrustedImm32(-1), Address(output, (i << 1) * sizeof(int)));
        }

        if (!m_pattern.m_body->m_hasFixedSize)
            setMatchStart(index);

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
            move(index, initialStart);

        generate();
        if (m_disassembler)
            m_disassembler->setEndOfGenerate(label());
        backtrack();
        if (m_disassembler)
            m_disassembler->setEndOfBacktrack(label());

        generateTryReadUnicodeCharacterHelper();

        generateJITFailReturn();

        if (m_disassembler)
            m_disassembler->setEndOfCode(label());

        LinkBuffer linkBuffer(*this, REGEXP_CODE_ID, JITCompilationCanFail);
        if (linkBuffer.didFailToAllocate()) {
            codeBlock.setFallBackWithFailureReason(JITFailureReason::ExecutableMemoryAllocationFailure);
            return;
        }

        if (!m_tryReadUnicodeCharacterCalls.isEmpty()) {
            CodeLocationLabel<NoPtrTag> tryReadUnicodeCharacterHelper = linkBuffer.locationOf<NoPtrTag>(m_tryReadUnicodeCharacterEntry);

            for (auto call : m_tryReadUnicodeCharacterCalls)
                linkBuffer.link(call, tryReadUnicodeCharacterHelper);
        }

        m_backtrackingState.linkDataLabels(linkBuffer);

        if (m_disassembler)
            m_disassembler->dump(linkBuffer);

        if (compileMode == MatchOnly) {
            if (m_charSize == Char8)
                codeBlock.set8BitCodeMatchOnly(FINALIZE_REGEXP_CODE(linkBuffer, YarrMatchOnly8BitPtrTag, "Match-only 8-bit regular expression"));
            else
                codeBlock.set16BitCodeMatchOnly(FINALIZE_REGEXP_CODE(linkBuffer, YarrMatchOnly16BitPtrTag, "Match-only 16-bit regular expression"));
        } else {
            if (m_charSize == Char8)
                codeBlock.set8BitCode(FINALIZE_REGEXP_CODE(linkBuffer, Yarr8BitPtrTag, "8-bit regular expression"));
            else
                codeBlock.set16BitCode(FINALIZE_REGEXP_CODE(linkBuffer, Yarr16BitPtrTag, "16-bit regular expression"));
        }
        if (m_failureReason)
            codeBlock.setFallBackWithFailureReason(*m_failureReason);
    }

    const char* variant() final
    {
        if (compileMode == MatchOnly) {
            if (m_charSize == Char8)
                return "Match-only 8-bit regular expression";

            return "Match-only 16-bit regular expression";
        }

        if (m_charSize == Char8)
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
        case OpTerm: {
            out.print("OpTerm ");
            switch (term->type) {
            case PatternTerm::TypeAssertionBOL:
                out.print("Assert BOL");
                break;

            case PatternTerm::TypeAssertionEOL:
                out.print("Assert EOL");
                break;

            case PatternTerm::TypeBackReference:
                out.printf("BackReference pattern #%u", term->backReferenceSubpatternId);
                term->dumpQuantifier(out);
                break;

            case PatternTerm::TypePatternCharacter:
                out.print("TypePatternCharacter ");
                dumpUChar32(out, term->patternCharacter);
                if (m_pattern.ignoreCase())
                    out.print(" ignore case");

                term->dumpQuantifier(out);
                break;

            case PatternTerm::TypeCharacterClass:
                out.print("TypePatternCharacterClass ");
                if (term->invert())
                    out.print("not ");
                dumpCharacterClass(out, &m_pattern, term->characterClass);
                term->dumpQuantifier(out);
                break;

            case PatternTerm::TypeAssertionWordBoundary:
                out.printf("%sword boundary", term->invert() ? "non-" : "");
                break;

            case PatternTerm::TypeDotStarEnclosure:
                out.print(".* enclosure");
                break;

            case PatternTerm::TypeForwardReference:
                out.print("TypeForwardReference <not handled>");
                break;

            case PatternTerm::TypeParenthesesSubpattern:
            case PatternTerm::TypeParentheticalAssertion:
                RELEASE_ASSERT_NOT_REACHED();
                break;
            }

            if (op.m_isDeadCode)
                out.print(" already handled");
            out.print("\n");
            return(0);
        }

        case OpBodyAlternativeBegin:
            out.printf("OpBodyAlternativeBegin minimum size %u\n", op.m_alternative->m_minimumSize);
            return(0);

        case OpBodyAlternativeNext:
            out.printf("OpBodyAlternativeNext minimum size %u\n", op.m_alternative->m_minimumSize);
            return(0);

        case OpBodyAlternativeEnd:
            out.print("OpBodyAlternativeEnd\n");
            return(0);

        case OpSimpleNestedAlternativeBegin:
            out.printf("OpSimpleNestedAlternativeBegin minimum size %u\n", op.m_alternative->m_minimumSize);
            return(1);

        case OpNestedAlternativeBegin:
            out.printf("OpNestedAlternativeBegin minimum size %u\n", op.m_alternative->m_minimumSize);
            return(1);

        case OpSimpleNestedAlternativeNext:
            out.printf("OpSimpleNestedAlternativeNext minimum size %u\n", op.m_alternative->m_minimumSize);
            return(0);

        case OpNestedAlternativeNext:
            out.printf("OpNestedAlternativeNext minimum size %u\n", op.m_alternative->m_minimumSize);
            return(0);

        case OpSimpleNestedAlternativeEnd:
            out.print("OpSimpleNestedAlternativeEnd");
            term->dumpQuantifier(out);
            out.print("\n");
            return(-1);

        case OpNestedAlternativeEnd:
            out.print("OpNestedAlternativeEnd");
            term->dumpQuantifier(out);
            out.print("\n");
            return(-1);

        case OpParenthesesSubpatternOnceBegin:
            out.print("OpParenthesesSubpatternOnceBegin ");
            if (term->capture())
                out.printf("capturing pattern #%u", term->parentheses.subpatternId);
            else
                out.print("non-capturing");
            term->dumpQuantifier(out);
            out.print("\n");
            return(0);

        case OpParenthesesSubpatternOnceEnd:
            out.print("OpParenthesesSubpatternOnceEnd ");
            if (term->capture())
                out.printf("capturing pattern #%u", term->parentheses.subpatternId);
            else
                out.print("non-capturing");
            term->dumpQuantifier(out);
            out.print("\n");
            return(0);

        case OpParenthesesSubpatternTerminalBegin:
            out.print("OpParenthesesSubpatternTerminalBegin ");
            if (term->capture())
                out.printf("capturing pattern #%u\n", term->parentheses.subpatternId);
            else
                out.print("non-capturing\n");
            return(0);

        case OpParenthesesSubpatternTerminalEnd:
            out.print("OpParenthesesSubpatternTerminalEnd ");
            if (term->capture())
                out.printf("capturing pattern #%u\n", term->parentheses.subpatternId);
            else
                out.print("non-capturing\n");
            return(0);

        case OpParenthesesSubpatternBegin:
            out.print("OpParenthesesSubpatternBegin ");
            if (term->capture())
                out.printf("capturing pattern #%u", term->parentheses.subpatternId);
            else
                out.print("non-capturing");
            term->dumpQuantifier(out);
            out.print("\n");
            return(0);

        case OpParenthesesSubpatternEnd:
            out.print("OpParenthesesSubpatternEnd ");
            if (term->capture())
                out.printf("capturing pattern #%u", term->parentheses.subpatternId);
            else
                out.print("non-capturing");
            term->dumpQuantifier(out);
            out.print("\n");
            return(0);

        case OpParentheticalAssertionBegin:
            out.printf("OpParentheticalAssertionBegin%s\n", term->invert() ? " inverted" : "");
            return(0);

        case OpParentheticalAssertionEnd:
            out.printf("OpParentheticalAssertionEnd%s\n", term->invert() ? " inverted" : "");
            return(0);

        case OpMatchFailed:
            out.print("OpMatchFailed\n");
            return(0);
        }

        return(0);
    }

private:
    VM* m_vm;

    YarrPattern& m_pattern;
    String& m_patternString;

    YarrCodeBlock& m_codeBlock;
    YarrCharSize m_charSize;

    // Used to detect regular expression constructs that are not currently
    // supported in the JIT; fall back to the interpreter when this is detected.
    Optional<JITFailureReason> m_failureReason;

    bool m_decodeSurrogatePairs;
    bool m_unicodeIgnoreCase;
    bool m_fixedSizedAlternative;
    CanonicalMode m_canonicalMode;
#if ENABLE(YARR_JIT_ALL_PARENS_EXPRESSIONS)
    bool m_containsNestedSubpatterns;
    ParenContextSizes m_parenContextSizes;
#endif
    JumpList m_abortExecution;
    JumpList m_hitMatchLimit;
    Vector<Call> m_tryReadUnicodeCharacterCalls;
    Label m_tryReadUnicodeCharacterEntry;

    // The regular expression expressed as a linear sequence of operations.
    Vector<YarrOp, 128> m_ops;

    // This records the current input offset being applied due to the current
    // set of alternatives we are nested within. E.g. when matching the
    // character 'b' within the regular expression /abc/, we will know that
    // the minimum size for the alternative is 3, checked upon entry to the
    // alternative, and that 'b' is at offset 1 from the start, and as such
    // when matching 'b' we need to apply an offset of -2 to the load.
    //
    // FIXME: This should go away. Rather than tracking this value throughout
    // code generation, we should gather this information up front & store it
    // on the YarrOp structure.
    Checked<unsigned> m_checkedOffset;

    // This class records state whilst generating the backtracking path of code.
    BacktrackingState m_backtrackingState;
    
    std::unique_ptr<YarrDisassembler> m_disassembler;
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
    }
}

void jitCompile(YarrPattern& pattern, String& patternString, YarrCharSize charSize, VM* vm, YarrCodeBlock& codeBlock, YarrJITCompileMode mode)
{
    if (mode == MatchOnly)
        YarrGenerator<MatchOnly>(vm, pattern, patternString, codeBlock, charSize).compile();
    else
        YarrGenerator<IncludeSubpatterns>(vm, pattern, patternString, codeBlock, charSize).compile();

    if (auto failureReason = codeBlock.failureReason()) {
        if (UNLIKELY(Options::dumpCompiledRegExpPatterns())) {
            pattern.dumpPatternString(WTF::dataFile(), patternString);
            dataLog(" : ");
            dumpCompileFailure(*failureReason);
        }
    }
}

}}

#endif
