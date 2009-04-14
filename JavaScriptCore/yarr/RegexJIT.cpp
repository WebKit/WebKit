/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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
#include "RegexJIT.h"

#include "ASCIICType.h"
#include "JSGlobalData.h"
#include "MacroAssembler.h"
#include "RegexCompiler.h"

#if ENABLE(YARR_JIT)

using namespace WTF;

namespace JSC { namespace Yarr {


class RegexGenerator : private MacroAssembler {
    friend void jitCompileRegex(JSGlobalData* globalData, RegexCodeBlock& jitObject, const UString& pattern, unsigned& numSubpatterns, const char*& error, bool ignoreCase, bool multiline);

    static const RegisterID returnRegister = X86::eax;
    static const RegisterID input = X86::eax;
    static const RegisterID index = X86::edx;
    static const RegisterID length = X86::ecx;
    static const RegisterID output = X86::edi;

    void optimizeAlternative(PatternAlternative* alternative)
    {
        if (!alternative->m_terms.size())
            return;

        for (unsigned i = 0; i < alternative->m_terms.size() - 1; ++i) {
            PatternTerm& term = alternative->m_terms[i];
            PatternTerm& nextTerm = alternative->m_terms[i + 1];

            if ((term.type == PatternTerm::TypeCharacterClass)
                && (term.quantityType == QuantifierFixedCount)
                && (nextTerm.type == PatternTerm::TypePatternCharacter)
                && (nextTerm.quantityType == QuantifierFixedCount)) {
                PatternTerm termCopy = term;
                alternative->m_terms[i] = nextTerm;
                alternative->m_terms[i + 1] = termCopy;
            }
        }
    }

    void matchCharacterClassRange(RegisterID character, JumpList& failures, JumpList& matchDest, const CharacterRange* ranges, unsigned count, unsigned* matchIndex, const UChar* matches, unsigned matchCount)
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
        Jump unicodeFail;
        if (charClass->m_matchesUnicode.size() || charClass->m_rangesUnicode.size()) {
            Jump isAscii = branch32(LessThanOrEqual, character, Imm32(0x7f));
        
            if (charClass->m_matchesUnicode.size()) {
                for (unsigned i = 0; i < charClass->m_matchesUnicode.size(); ++i) {
                    UChar ch = charClass->m_matchesUnicode[i];
                    matchDest.append(branch32(Equal, character, Imm32(ch)));
                }
            }
            
            if (charClass->m_rangesUnicode.size()) {
                for (unsigned i = 0; i < charClass->m_rangesUnicode.size(); ++i) {
                    UChar lo = charClass->m_rangesUnicode[i].begin;
                    UChar hi = charClass->m_rangesUnicode[i].end;
                    
                    Jump below = branch32(LessThan, character, Imm32(lo));
                    matchDest.append(branch32(LessThanOrEqual, character, Imm32(hi)));
                    below.link(this);
                }
            }

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
                if (m_pattern.m_ignoreCase) {
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
                or32(Imm32(32), character);
                for (unsigned i = 0; i < countAZaz; ++i)
                    matchDest.append(branch32(Equal, character, Imm32(matchesAZaz[i])));
            }
        }

        if (charClass->m_matchesUnicode.size() || charClass->m_rangesUnicode.size())
            unicodeFail.link(this);
    }

    // Jumps if input not available; will have (incorrectly) incremented already!
    Jump jumpIfNoAvailableInput(unsigned countToCheck)
    {
        add32(Imm32(countToCheck), index);
        return branch32(Above, index, length);
    }

    Jump jumpIfAvailableInput(unsigned countToCheck)
    {
        add32(Imm32(countToCheck), index);
        return branch32(BelowOrEqual, index, length);
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

    Jump jumpIfCharEquals(UChar ch, int inputPosition)
    {
        return branch16(Equal, BaseIndex(input, index, TimesTwo, inputPosition * sizeof(UChar)), Imm32(ch));
    }

    Jump jumpIfCharNotEquals(UChar ch, int inputPosition)
    {
        return branch16(NotEqual, BaseIndex(input, index, TimesTwo, inputPosition * sizeof(UChar)), Imm32(ch));
    }

    void readCharacter(int inputPosition, RegisterID reg)
    {
        load16(BaseIndex(input, index, TimesTwo, inputPosition * sizeof(UChar)), reg);
    }

    void storeToFrame(RegisterID reg, unsigned frameLocation)
    {
        poke(reg, frameLocation);
    }

    void loadFromFrame(unsigned frameLocation, RegisterID reg)
    {
        peek(reg, frameLocation);
    }

    struct TermGenerationState {
        TermGenerationState(PatternDisjunction* disjunction, unsigned checkedTotal)
            : disjunction(disjunction)
            , checkedTotal(checkedTotal)
        {
        }

        void resetAlternative()
        {
            alt = 0;
        }
        bool alternativeValid()
        {
            return alt < disjunction->m_alternatives.size();
        }
        void nextAlternative()
        {
            ++alt;
        }
        PatternAlternative* alternative()
        {
            return disjunction->m_alternatives[alt];
        }

        void resetTerm()
        {
            ASSERT(alternativeValid());
            t = 0;
            isBackTrackGenerated = false;
        }
        bool termValid()
        {
            ASSERT(alternativeValid());
            return t < alternative()->m_terms.size();
        }
        void nextTerm()
        {
            ASSERT(alternativeValid());
            ++t;
        }
        PatternTerm& term()
        {
            ASSERT(alternativeValid());
            return alternative()->m_terms[t];
        }

        PatternTerm& lookaheadTerm()
        {
            ASSERT(alternativeValid());
            ASSERT((t + 1) < alternative()->m_terms.size());
            return alternative()->m_terms[t + 1];
        }
        bool isSinglePatternCharacterLookaheadTerm()
        {
            ASSERT(alternativeValid());
            return ((t + 1) < alternative()->m_terms.size())
                && (lookaheadTerm().type == PatternTerm::TypePatternCharacter)
                && (lookaheadTerm().quantityType == QuantifierFixedCount)
                && (lookaheadTerm().quantityCount == 1);
        }

        int inputOffset()
        {
            return term().inputPosition - checkedTotal;
        }

        void jumpToBacktrack(Jump jump, MacroAssembler* masm)
        {
           if (isBackTrackGenerated)
                jump.linkTo(backtrackLabel, masm);
            else
                jumpsToNextAlternative.append(jump);
        }
        void jumpToBacktrack(JumpList& jumps, MacroAssembler* masm)
        {
           if (isBackTrackGenerated)
                jumps.linkTo(backtrackLabel, masm);
            else
                jumpsToNextAlternative.append(jumps);
        }
        void setBacktrackGenerated(Label label)
        {
            isBackTrackGenerated = true;
            backtrackLabel = label;
        }

        PatternDisjunction* disjunction;
        int checkedTotal;
        unsigned alt;
        unsigned t;
        JumpList jumpsToNextAlternative;
        Label backtrackLabel;
        bool isBackTrackGenerated;
    };

    void jumpToBacktrackCheckEmitPending(TermGenerationState& state, Jump backtrackMe)
    {
        state.jumpToBacktrack(backtrackMe, this);
    }

    void jumpToBacktrackCheckEmitPending(TermGenerationState& state, JumpList& backtrackMe)
    {
        state.jumpToBacktrack(backtrackMe, this);
    }

    void genertateAssertionBOL(TermGenerationState& state)
    {
        PatternTerm& term = state.term();

        if (m_pattern.m_multiline) {
            const RegisterID character = X86::esi;

            JumpList matchDest;
            if (!term.inputPosition)
                matchDest.append(branch32(Equal, index, Imm32(state.checkedTotal)));

            readCharacter(state.inputOffset() - 1, character);
            matchCharacterClass(character, matchDest, m_pattern.newlineCharacterClass());
            jumpToBacktrackCheckEmitPending(state, jump());

            matchDest.link(this);
        } else {
            // Erk, really should poison out these alternatives early. :-/
            if (term.inputPosition)
                jumpToBacktrackCheckEmitPending(state, jump());
            else
                jumpToBacktrackCheckEmitPending(state, branch32(NotEqual, index, Imm32(state.checkedTotal)));
        }
    }

    void genertateAssertionEOL(TermGenerationState& state)
    {
        PatternTerm& term = state.term();

        if (m_pattern.m_multiline) {
            const RegisterID character = X86::esi;

            JumpList matchDest;
            if (term.inputPosition == state.checkedTotal)
                matchDest.append(atEndOfInput());

            readCharacter(state.inputOffset(), character);
            matchCharacterClass(character, matchDest, m_pattern.newlineCharacterClass());
            jumpToBacktrackCheckEmitPending(state, jump());

            matchDest.link(this);
        } else {
            if (term.inputPosition == state.checkedTotal)
                jumpToBacktrackCheckEmitPending(state, notAtEndOfInput());
            // Erk, really should poison out these alternatives early. :-/
            else
                jumpToBacktrackCheckEmitPending(state, jump());
        }
    }

    // Also falls though on nextIsNotWordChar.
    void matchAssertionWordchar(TermGenerationState& state, JumpList& nextIsWordChar, JumpList& nextIsNotWordChar)
    {
        const RegisterID character = X86::esi;
        PatternTerm& term = state.term();

        if (term.inputPosition == state.checkedTotal)
            nextIsNotWordChar.append(atEndOfInput());

        readCharacter(state.inputOffset(), character);
        matchCharacterClass(character, nextIsWordChar, m_pattern.wordcharCharacterClass());
    }

    void genertateAssertionWordBoundary(TermGenerationState& state)
    {
        const RegisterID character = X86::esi;
        PatternTerm& term = state.term();

        Jump atBegin;
        JumpList matchDest;
        if (!term.inputPosition)
            atBegin = branch32(Equal, index, Imm32(state.checkedTotal));
        readCharacter(state.inputOffset() - 1, character);
        matchCharacterClass(character, matchDest, m_pattern.wordcharCharacterClass());
        if (!term.inputPosition)
            atBegin.link(this);

        // We fall through to here if the last character was not a wordchar.
        JumpList nonWordCharThenWordChar;
        JumpList nonWordCharThenNonWordChar;
        if (term.invertOrCapture) {
            matchAssertionWordchar(state, nonWordCharThenNonWordChar, nonWordCharThenWordChar);
            nonWordCharThenWordChar.append(jump());
        } else {
            matchAssertionWordchar(state, nonWordCharThenWordChar, nonWordCharThenNonWordChar);
            nonWordCharThenNonWordChar.append(jump());
        }
        jumpToBacktrackCheckEmitPending(state, nonWordCharThenNonWordChar);

        // We jump here if the last character was a wordchar.
        matchDest.link(this);
        JumpList wordCharThenWordChar;
        JumpList wordCharThenNonWordChar;
        if (term.invertOrCapture) {
            matchAssertionWordchar(state, wordCharThenNonWordChar, wordCharThenWordChar);
            wordCharThenWordChar.append(jump());
        } else {
            matchAssertionWordchar(state, wordCharThenWordChar, wordCharThenNonWordChar);
            // This can fall-though!
        }

        jumpToBacktrackCheckEmitPending(state, wordCharThenWordChar);
        
        nonWordCharThenWordChar.link(this);
        wordCharThenNonWordChar.link(this);
    }

    void genertatePatternCharacterSingle(TermGenerationState& state)
    {
        const RegisterID character = X86::esi;
        UChar ch = state.term().patternCharacter;

        if (m_pattern.m_ignoreCase && isASCIIAlpha(ch)) {
            readCharacter(state.inputOffset(), character);
            or32(Imm32(32), character);
            jumpToBacktrackCheckEmitPending(state, branch32(NotEqual, character, Imm32(Unicode::toLower(ch))));
        } else {
            ASSERT(!m_pattern.m_ignoreCase || (Unicode::toLower(ch) == Unicode::toUpper(ch)));
            jumpToBacktrackCheckEmitPending(state, jumpIfCharNotEquals(ch, state.inputOffset()));
        }
    }

    void genertatePatternCharacterPair(TermGenerationState& state)
    {
        const RegisterID character = X86::esi;
        UChar ch1 = state.term().patternCharacter;
        UChar ch2 = state.lookaheadTerm().patternCharacter;

        int mask = 0;
        int chPair = ch1 | (ch2 << 16);
        
        if (m_pattern.m_ignoreCase) {
            if (isASCIIAlpha(ch1))
                mask |= 32;
            if (isASCIIAlpha(ch2))
                mask |= 32 << 16;
        }

        if (mask) {
            load32(BaseIndex(input, index, TimesTwo, state.inputOffset() * sizeof(UChar)), character);
            or32(Imm32(mask), character);
            jumpToBacktrackCheckEmitPending(state, branch32(NotEqual, character, Imm32(chPair | mask)));
        } else
            jumpToBacktrackCheckEmitPending(state, branch32(NotEqual, BaseIndex(input, index, TimesTwo, state.inputOffset() * sizeof(UChar)), Imm32(chPair)));
    }

    void genertatePatternCharacterFixed(TermGenerationState& state)
    {
        const RegisterID character = X86::esi;
        const RegisterID countRegister = X86::ebx;
        PatternTerm& term = state.term();
        UChar ch = term.patternCharacter;

        move(index, countRegister);
        sub32(Imm32(term.quantityCount), countRegister);

        Label loop(this);
        if (m_pattern.m_ignoreCase && isASCIIAlpha(ch)) {
            load16(BaseIndex(input, countRegister, TimesTwo, (state.inputOffset() + term.quantityCount) * sizeof(UChar)), character);
            or32(Imm32(32), character);
            jumpToBacktrackCheckEmitPending(state, branch32(NotEqual, character, Imm32(Unicode::toLower(ch))));
        } else {
            ASSERT(!m_pattern.m_ignoreCase || (Unicode::toLower(ch) == Unicode::toUpper(ch)));
            jumpToBacktrackCheckEmitPending(state, branch16(NotEqual, BaseIndex(input, countRegister, TimesTwo, (state.inputOffset() + term.quantityCount) * sizeof(UChar)), Imm32(ch)));
        }
        add32(Imm32(1), countRegister);
        branch32(NotEqual, countRegister, index).linkTo(loop, this);
    }

    void genertatePatternCharacterGreedy(TermGenerationState& state)
    {
        const RegisterID character = X86::esi;
        const RegisterID countRegister = X86::ebx;
        PatternTerm& term = state.term();
        UChar ch = term.patternCharacter;
    
        move(Imm32(0), countRegister);

        JumpList failures;
        Label loop(this);
        failures.append(atEndOfInput());
        if (m_pattern.m_ignoreCase && isASCIIAlpha(ch)) {
            readCharacter(state.inputOffset(), character);
            or32(Imm32(32), character);
            failures.append(branch32(NotEqual, character, Imm32(Unicode::toLower(ch))));
        } else {
            ASSERT(!m_pattern.m_ignoreCase || (Unicode::toLower(ch) == Unicode::toUpper(ch)));
            failures.append(jumpIfCharNotEquals(ch, state.inputOffset()));
        }
        add32(Imm32(1), countRegister);
        add32(Imm32(1), index);
        branch32(NotEqual, countRegister, Imm32(term.quantityCount)).linkTo(loop, this);
        failures.append(jump());

        Label backtrackBegin(this);
        loadFromFrame(term.frameLocation, countRegister);
        jumpToBacktrackCheckEmitPending(state, branchTest32(Zero, countRegister));
        sub32(Imm32(1), countRegister);
        sub32(Imm32(1), index);

        failures.link(this);

        storeToFrame(countRegister, term.frameLocation);

        state.setBacktrackGenerated(backtrackBegin);
    }

    void genertatePatternCharacterNonGreedy(TermGenerationState& state)
    {
        const RegisterID character = X86::esi;
        const RegisterID countRegister = X86::ebx;
        PatternTerm& term = state.term();
        UChar ch = term.patternCharacter;
    
        move(Imm32(0), countRegister);

        Jump firstTimeDoNothing = jump();

        Label hardFail(this);
        sub32(countRegister, index);
        jumpToBacktrackCheckEmitPending(state, jump());

        Label backtrackBegin(this);
        loadFromFrame(term.frameLocation, countRegister);

        atEndOfInput().linkTo(hardFail, this);
        branch32(Equal, countRegister, Imm32(term.quantityCount), hardFail);
        if (m_pattern.m_ignoreCase && isASCIIAlpha(ch)) {
            readCharacter(state.inputOffset(), character);
            or32(Imm32(32), character);
            branch32(NotEqual, character, Imm32(Unicode::toLower(ch))).linkTo(hardFail, this);
        } else {
            ASSERT(!m_pattern.m_ignoreCase || (Unicode::toLower(ch) == Unicode::toUpper(ch)));
            jumpIfCharNotEquals(ch, state.inputOffset()).linkTo(hardFail, this);
        }

        add32(Imm32(1), countRegister);
        add32(Imm32(1), index);

        firstTimeDoNothing.link(this);
        storeToFrame(countRegister, term.frameLocation);

        state.setBacktrackGenerated(backtrackBegin);
    }

    void genertateCharacterClassSingle(TermGenerationState& state)
    {
        const RegisterID character = X86::esi;
        PatternTerm& term = state.term();

        JumpList matchDest;
        readCharacter(state.inputOffset(), character);
        matchCharacterClass(character, matchDest, term.characterClass);

        if (term.invertOrCapture)
            jumpToBacktrackCheckEmitPending(state, matchDest);
        else {
            jumpToBacktrackCheckEmitPending(state, jump());
            matchDest.link(this);
        }
    }

    void genertateCharacterClassFixed(TermGenerationState& state)
    {
        const RegisterID character = X86::esi;
        const RegisterID countRegister = X86::ebx;
        PatternTerm& term = state.term();

        move(index, countRegister);
        sub32(Imm32(term.quantityCount), countRegister);

        Label loop(this);
        JumpList matchDest;
        load16(BaseIndex(input, countRegister, TimesTwo, (state.inputOffset() + term.quantityCount) * sizeof(UChar)), character);
        matchCharacterClass(character, matchDest, term.characterClass);

        if (term.invertOrCapture)
            jumpToBacktrackCheckEmitPending(state, matchDest);
        else {
            jumpToBacktrackCheckEmitPending(state, jump());
            matchDest.link(this);
        }

        add32(Imm32(1), countRegister);
        branch32(NotEqual, countRegister, index).linkTo(loop, this);
    }

    void genertateCharacterClassGreedy(TermGenerationState& state)
    {
        const RegisterID character = X86::esi;
        const RegisterID countRegister = X86::ebx;
        PatternTerm& term = state.term();
    
        move(Imm32(0), countRegister);

        JumpList failures;
        Label loop(this);
        failures.append(atEndOfInput());

        if (term.invertOrCapture) {
            readCharacter(state.inputOffset(), character);
            matchCharacterClass(character, failures, term.characterClass);
        } else {
            JumpList matchDest;
            readCharacter(state.inputOffset(), character);
            matchCharacterClass(character, matchDest, term.characterClass);
            failures.append(jump());
            matchDest.link(this);
        }

        add32(Imm32(1), countRegister);
        add32(Imm32(1), index);
        branch32(NotEqual, countRegister, Imm32(term.quantityCount)).linkTo(loop, this);
        failures.append(jump());

        Label backtrackBegin(this);
        loadFromFrame(term.frameLocation, countRegister);
        jumpToBacktrackCheckEmitPending(state, branchTest32(Zero, countRegister));
        sub32(Imm32(1), countRegister);
        sub32(Imm32(1), index);

        failures.link(this);

        storeToFrame(countRegister, term.frameLocation);

        state.setBacktrackGenerated(backtrackBegin);
    }

    void genertateCharacterClassNonGreedy(TermGenerationState& state)
    {
        const RegisterID character = X86::esi;
        const RegisterID countRegister = X86::ebx;
        PatternTerm& term = state.term();
    
        move(Imm32(0), countRegister);

        Jump firstTimeDoNothing = jump();

        Label hardFail(this);
        sub32(countRegister, index);
        jumpToBacktrackCheckEmitPending(state, jump());

        Label backtrackBegin(this);
        loadFromFrame(term.frameLocation, countRegister);

        atEndOfInput().linkTo(hardFail, this);
        branch32(Equal, countRegister, Imm32(term.quantityCount), hardFail);

        JumpList matchDest;
        readCharacter(state.inputOffset(), character);
        matchCharacterClass(character, matchDest, term.characterClass);

        if (term.invertOrCapture)
            matchDest.linkTo(hardFail, this);
        else {
            jump(hardFail);
            matchDest.link(this);
        }

        add32(Imm32(1), countRegister);
        add32(Imm32(1), index);

        firstTimeDoNothing.link(this);
        storeToFrame(countRegister, term.frameLocation);

        state.setBacktrackGenerated(backtrackBegin);
    }

    void generateParenthesesSingleDisjunctionOneAlternative(TermGenerationState& outterState)
    {
        PatternTerm& parenthesesTerm = outterState.term();
        PatternDisjunction* disjunction = parenthesesTerm.parentheses.disjunction;

        if (parenthesesTerm.invertOrCapture) {
            move(index, X86::esi);
            add32(Imm32(outterState.inputOffset() - disjunction->m_minimumSize), X86::esi);
            store32(X86::esi, Address(output, (parenthesesTerm.parentheses.subpatternId << 1) * sizeof(int)));
        }

        TermGenerationState state(disjunction, outterState.checkedTotal);

        state.resetAlternative();
        ASSERT(state.alternativeValid());
        PatternAlternative* alternative = state.alternative();
        optimizeAlternative(alternative);

        // Nothing to check!
        ASSERT(alternative->m_minimumSize == parenthesesTerm.parentheses.disjunction->m_minimumSize);

        for (state.resetTerm(); state.termValid(); state.nextTerm())
            generateTerm(state);
        // if we fall through to here, the parenthese were successfully matched.

        if (parenthesesTerm.invertOrCapture) {
            move(index, X86::esi);
            add32(Imm32(outterState.inputOffset()), X86::esi);
            store32(X86::esi, Address(output, ((parenthesesTerm.parentheses.subpatternId << 1) + 1) * sizeof(int)));
            Jump overBacktrack = jump();
            
            Label backtrackFromAfterParens(this);
            store32(Imm32(-1), Address(output, ((parenthesesTerm.parentheses.subpatternId << 1) + 1) * sizeof(int)));
            if (state.isBackTrackGenerated)
                jump(state.backtrackLabel);
            state.jumpsToNextAlternative.link(this);
            store32(Imm32(-1), Address(output, (parenthesesTerm.parentheses.subpatternId << 1) * sizeof(int)));

            outterState.setBacktrackGenerated(backtrackFromAfterParens);
            overBacktrack.link(this);
        } else {
            // If these parens don't capture, just sew the chain of backtracking into the outter alternative.
            jumpToBacktrackCheckEmitPending(outterState, state.jumpsToNextAlternative);
            if (state.isBackTrackGenerated)
                outterState.setBacktrackGenerated(state.backtrackLabel);
        }
    }

    void generateParenthesesSingle(TermGenerationState& state)
    {
        PatternTerm& term = state.term();
        ASSERT(term.quantityCount == 1);
        
        if ((term.quantityType == QuantifierFixedCount) && (term.parentheses.disjunction->m_alternatives.size() == 1))
            generateParenthesesSingleDisjunctionOneAlternative(state);
        else {
            ASSERT_NOT_REACHED();
        }
    }

    void generateTerm(TermGenerationState& state)
    {
        PatternTerm& term = state.term();

        switch (term.type) {
        case PatternTerm::TypeAssertionBOL:
            genertateAssertionBOL(state);
            break;
        
        case PatternTerm::TypeAssertionEOL:
            genertateAssertionEOL(state);
            break;
        
        case PatternTerm::TypeAssertionWordBoundary:
            genertateAssertionWordBoundary(state);
            break;
        
        case PatternTerm::TypePatternCharacter:
            switch (term.quantityType) {
            case QuantifierFixedCount:
                if (term.quantityCount == 1) {
                    if (state.isSinglePatternCharacterLookaheadTerm() && (state.lookaheadTerm().inputPosition == (term.inputPosition + 1))) {
                        genertatePatternCharacterPair(state);
                        state.nextTerm();
                    } else
                        genertatePatternCharacterSingle(state);
                } else
                    genertatePatternCharacterFixed(state);
                break;
            case QuantifierGreedy:
                genertatePatternCharacterGreedy(state);
                break;
            case QuantifierNonGreedy:
                genertatePatternCharacterNonGreedy(state);
                break;
            }
            break;

        case PatternTerm::TypeCharacterClass:
            switch (term.quantityType) {
            case QuantifierFixedCount:
                if (term.quantityCount == 1)
                    genertateCharacterClassSingle(state);
                else
                    genertateCharacterClassFixed(state);
                break;
            case QuantifierGreedy:
                genertateCharacterClassGreedy(state);
                break;
            case QuantifierNonGreedy:
                genertateCharacterClassNonGreedy(state);
                break;
            }
            break;

        case PatternTerm::TypeBackReference:
            ASSERT_NOT_REACHED();

        case PatternTerm::TypeParenthesesSubpattern:
            if (term.quantityCount == 1)
                generateParenthesesSingle(state);
            else
                ASSERT_NOT_REACHED();

        case PatternTerm::TypeParentheticalAssertion:
            ASSERT_NOT_REACHED();
        }
    }

    void generateDisjunction(PatternDisjunction* disjunction)
    {
        TermGenerationState state(disjunction, 0);

        Label firstAlternative(this);

        for (state.resetAlternative(); state.alternativeValid(); state.nextAlternative()) {
            PatternAlternative* alternative = state.alternative();

            optimizeAlternative(alternative);

            // check availability for the first alternative
            int countToCheck = (alternative->m_minimumSize);
            ASSERT(countToCheck >= 0);
            if (countToCheck) {
                state.jumpsToNextAlternative.append(jumpIfNoAvailableInput(countToCheck));
                state.checkedTotal += countToCheck;
            }

            for (state.resetTerm(); state.termValid(); state.nextTerm())
                generateTerm(state);

            if (m_pattern.m_body->m_callFrameSize)
                addPtr(Imm32(m_pattern.m_body->m_callFrameSize * sizeof(void*)), stackPointerRegister);
            
            // If we get here, the alternative matched.
            ASSERT(index != returnRegister);
            if (m_pattern.m_body->m_hasFixedSize) {
                store32(index, Address(output, 4)); // match end
                sub32(Imm32(alternative->m_minimumSize), index);
                store32(index, output);
            } else {
                pop(returnRegister);
                store32(returnRegister, output);
                store32(index, Address(output, 4)); // match end
            }

            // Restore callee save registers.
            pop(X86::esi);
            pop(X86::edi);
            pop(X86::ebx);
            pop(X86::ebp);
            ret();

            state.jumpsToNextAlternative.link(this);

            if (countToCheck) {
                sub32(Imm32(countToCheck), index);
                state.checkedTotal -= countToCheck;
            }
        }
        
        // If we get here, all Alternatives failed.

        add32(Imm32(1), index);
        if (!m_pattern.m_body->m_hasFixedSize)
            poke(index, m_pattern.m_body->m_callFrameSize);
        checkInput().linkTo(firstAlternative, this);

        unsigned frameSize = m_pattern.m_body->m_callFrameSize;
        if (!m_pattern.m_body->m_hasFixedSize)
            ++frameSize;
        if (frameSize)
            addPtr(Imm32(frameSize * sizeof(void*)), stackPointerRegister);

        move(Imm32(-1), returnRegister);

        pop(X86::esi);
        pop(X86::edi);
        pop(X86::ebx);
        pop(X86::ebp);
        ret();
    }

public:
    RegexGenerator(RegexPattern& pattern)
        : m_pattern(pattern)
    {
    }

    void generate()
    {
        // On x86 edi & esi are callee preserved registers.
        push(X86::ebp);
        move(X86::esp, X86::ebp);
        push(X86::ebx);
        push(X86::edi);
        push(X86::esi);
        // load output into edi (2 = saved ebp + return address).
        loadPtr(Address(X86::ebp, 2 * sizeof(void*)), output);
        // TODO: do we need spill registers to fill the output pointer if
        // there are no sub captures? - we should not need 

        // TODO: do I really want this on the stack?
        if (!m_pattern.m_body->m_hasFixedSize)
            push(index);

        if (m_pattern.m_body->m_callFrameSize)
            subPtr(Imm32(m_pattern.m_body->m_callFrameSize * sizeof(void*)), stackPointerRegister);

        generateDisjunction(m_pattern.m_body);
    }

    RegexPattern& m_pattern;
};

void jitCompileRegex(JSGlobalData* globalData, RegexCodeBlock& jitObject, const UString& patternString, unsigned& numSubpatterns, const char*& error, bool ignoreCase, bool multiline)
{
    RegexPattern pattern(ignoreCase, multiline);

    if ((error = compileRegex(patternString, pattern)))
        return;

    numSubpatterns = pattern.m_numSubpatterns;

    RegexGenerator generator(pattern);
    generator.generate();

    jitObject.m_executablePool = globalData->executableAllocator.poolForSize(generator.size());
    jitObject.m_jitCode = generator.copyCode(jitObject.m_executablePool.get());
}

int executeRegex(RegexCodeBlock& jitObject, const UChar* input, unsigned start, unsigned length, int* output)
{
    typedef int (*RegexJITCode)(const UChar* input, unsigned start, unsigned length, int* output) __attribute__ ((regparm (3)));

    int result = reinterpret_cast<RegexJITCode>(jitObject.m_jitCode)(input, start, length, output);

    return result;
}

}}

#endif





