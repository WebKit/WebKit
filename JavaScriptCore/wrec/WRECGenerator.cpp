/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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
#include "WREC.h"

#if ENABLE(WREC)

#include "CharacterClassConstructor.h"
#include "Interpreter.h"
#include "WRECFunctors.h"
#include "WRECParser.h"
#include "pcre_internal.h"

using namespace WTF;

namespace JSC { namespace WREC {

void Generator::generateEnter()
{
#if PLATFORM(X86)
    // On x86 edi & esi are callee preserved registers.
    push(X86::edi);
    push(X86::esi);
    
#if COMPILER(MSVC)
    // Move the arguments into registers.
    peek(input, 3);
    peek(index, 4);
    peek(length, 5);
    peek(output, 6);
#else
    // On gcc the function is regparm(3), so the input, index, and length registers
    // (eax, edx, and ecx respectively) already contain the appropriate values.
    // Just load the fourth argument (output) into edi
    peek(output, 3);
#endif
#endif
}

void Generator::generateReturnSuccess()
{
    ASSERT(returnRegister != index);
    ASSERT(returnRegister != output);

    // Set return value.
    pop(returnRegister); // match begin
    store32(returnRegister, output);
    store32(index, Address(output, 4)); // match end
    
    // Restore callee save registers.
#if PLATFORM(X86)
    pop(X86::esi);
    pop(X86::edi);
#endif
    ret();
}

void Generator::generateSaveIndex()
{
    push(index);
}

void Generator::generateIncrementIndex(Jump* failure)
{
    peek(index);
    if (failure)
        *failure = branch32(Equal, length, index);
    add32(Imm32(1), index);
    poke(index);
}

void Generator::generateLoadCharacter(JumpList& failures)
{
    failures.append(branch32(Equal, length, index));
    load16(BaseIndex(input, index, TimesTwo), character);
}

// For the sake of end-of-line assertions, we treat one-past-the-end as if it
// were part of the input string.
void Generator::generateJumpIfNotEndOfInput(Label target)
{
    branch32(LessThanOrEqual, index, length, target);
}

void Generator::generateReturnFailure()
{
    pop();
    move(Imm32(-1), returnRegister);

#if PLATFORM(X86)
    pop(X86::esi);
    pop(X86::edi);
#endif
    ret();
}

void Generator::generateBacktrack1()
{
    sub32(Imm32(1), index);
}

void Generator::generateBacktrackBackreference(unsigned subpatternId)
{
    sub32(Address(output, (2 * subpatternId + 1) * sizeof(int)), index);
    add32(Address(output, (2 * subpatternId) * sizeof(int)), index);
}

void Generator::generateBackreferenceQuantifier(JumpList& failures, Quantifier::Type quantifierType, unsigned subpatternId, unsigned min, unsigned max)
{
    GenerateBackreferenceFunctor functor(subpatternId);

    load32(Address(output, (2 * subpatternId) * sizeof(int)), character);
    Jump skipIfEmpty = branch32(Equal, Address(output, ((2 * subpatternId) + 1) * sizeof(int)), character);

    ASSERT(quantifierType == Quantifier::Greedy || quantifierType == Quantifier::NonGreedy);
    if (quantifierType == Quantifier::Greedy)
        generateGreedyQuantifier(failures, functor, min, max);
    else
        generateNonGreedyQuantifier(failures, functor, min, max);

    skipIfEmpty.link(this);
}

void Generator::generateNonGreedyQuantifier(JumpList& failures, GenerateAtomFunctor& functor, unsigned min, unsigned max)
{
    JumpList atomFailedList;
    JumpList alternativeFailedList;

    // (0) Setup: Save, then init repeatCount.
    push(repeatCount);
    move(Imm32(0), repeatCount);
    Jump start = jump();

    // (4) Quantifier failed: No more atom reading possible.
    Label quantifierFailed(this);
    pop(repeatCount);
    failures.append(jump()); 

    // (3) Alternative failed: If we can, read another atom, then fall through to (2) to try again.
    Label alternativeFailed(this);
    pop(index);
    if (max != Quantifier::Infinity)
        branch32(Equal, repeatCount, Imm32(max), quantifierFailed);

    // (1) Read an atom.
    if (min)
        start.link(this);
    Label readAtom(this);
    functor.generateAtom(this, atomFailedList);
    atomFailedList.linkTo(quantifierFailed, this);
    add32(Imm32(1), repeatCount);
    
    // (2) Keep reading if we're under the minimum.
    if (min > 1)
        branch32(LessThan, repeatCount, Imm32(min), readAtom);

    // (3) Test the rest of the alternative.
    if (!min)
        start.link(this);
    push(index);
    m_parser.parseAlternative(alternativeFailedList);
    alternativeFailedList.linkTo(alternativeFailed, this);

    pop();
    pop(repeatCount);
}

void Generator::generateGreedyQuantifier(JumpList& failures, GenerateAtomFunctor& functor, unsigned min, unsigned max)
{
    if (!max)
        return;

    JumpList doneReadingAtomsList;
    JumpList alternativeFailedList;

    // (0) Setup: Save, then init repeatCount.
    push(repeatCount);
    move(Imm32(0), repeatCount);

    // (1) Greedily read as many copies of the atom as possible, then jump to (2).
    Label readAtom(this);
    functor.generateAtom(this, doneReadingAtomsList);
    add32(Imm32(1), repeatCount);
    if (max == Quantifier::Infinity)
        jump(readAtom);
    else if (max == 1)
        doneReadingAtomsList.append(jump());
    else {
        branch32(NotEqual, repeatCount, Imm32(max), readAtom);
        doneReadingAtomsList.append(jump());
    }

    // (5) Quantifier failed: No more backtracking possible.
    Label quantifierFailed(this);
    pop(repeatCount);
    failures.append(jump()); 

    // (4) Alternative failed: Backtrack, then fall through to (2) to try again.
    Label alternativeFailed(this);
    pop(index);
    functor.backtrack(this);
    sub32(Imm32(1), repeatCount);

    // (2) Verify that we have enough atoms.
    doneReadingAtomsList.link(this);
    branch32(LessThan, repeatCount, Imm32(min), quantifierFailed);

    // (3) Test the rest of the alternative.
    push(index);
    m_parser.parseAlternative(alternativeFailedList);
    alternativeFailedList.linkTo(alternativeFailed, this);

    pop();
    pop(repeatCount);
}

void Generator::generatePatternCharacterSequence(JumpList& failures, int* sequence, size_t count)
{
    for (size_t i = 0; i < count;) {
        if (i < count - 1) {
            if (generatePatternCharacterPair(failures, sequence[i], sequence[i + 1])) {
                i += 2;
                continue;
            }
        }

        generatePatternCharacter(failures, sequence[i]);
        ++i;
    }
}

bool Generator::generatePatternCharacterPair(JumpList& failures, int ch1, int ch2)
{
    if (m_parser.ignoreCase()) {
        // Non-trivial case folding requires more than one test, so we can't
        // test as a pair with an adjacent character.
        if (!isASCII(ch1) && Unicode::toLower(ch1) != Unicode::toUpper(ch1))
            return false;
        if (!isASCII(ch2) && Unicode::toLower(ch2) != Unicode::toUpper(ch2))
            return false;
    }

    // Optimistically consume 2 characters.
    add32(Imm32(2), index);
    failures.append(branch32(GreaterThan, index, length));

    // Load the characters we just consumed, offset -2 characters from index.
    load32(BaseIndex(input, index, TimesTwo, -2 * 2), character);

    if (m_parser.ignoreCase()) {
        // Convert ASCII alphabet characters to upper case before testing for
        // equality. (ASCII non-alphabet characters don't require upper-casing
        // because they have no uppercase equivalents. Unicode characters don't
        // require upper-casing because we only handle Unicode characters whose
        // upper and lower cases are equal.)
        int ch1Mask = 0;
        if (isASCIIAlpha(ch1)) {
            ch1 |= 32;
            ch1Mask = 32;
        }

        int ch2Mask = 0;
        if (isASCIIAlpha(ch2)) {
            ch2 |= 32;
            ch2Mask = 32;
        }

        int mask = ch1Mask | (ch2Mask << 16);
        if (mask)
            or32(Imm32(mask), character);
    }
    int pair = ch1 | (ch2 << 16);

    failures.append(branch32(NotEqual, character, Imm32(pair)));
    return true;
}

void Generator::generatePatternCharacter(JumpList& failures, int ch)
{
    generateLoadCharacter(failures);

    // used for unicode case insensitive
    bool hasUpper = false;
    Jump isUpper;
    
    // if case insensitive match
    if (m_parser.ignoreCase()) {
        UChar lower, upper;
        
        // check for ascii case sensitive characters
        if (isASCIIAlpha(ch)) {
            or32(Imm32(32), character);
            ch |= 32;
        } else if (!isASCII(ch) && ((lower = Unicode::toLower(ch)) != (upper = Unicode::toUpper(ch)))) {
            // handle unicode case sentitive characters - branch to success on upper
            isUpper = branch32(Equal, character, Imm32(upper));
            hasUpper = true;
            ch = lower;
        }
    }
    
    // checks for ch, or lower case version of ch, if insensitive
    failures.append(branch32(NotEqual, character, Imm32((unsigned short)ch)));

    if (m_parser.ignoreCase() && hasUpper) {
        // for unicode case insensitive matches, branch here if upper matches.
        isUpper.link(this);
    }
    
    // on success consume the char
    add32(Imm32(1), index);
}

void Generator::generateCharacterClassInvertedRange(JumpList& failures, JumpList& matchDest, const CharacterRange* ranges, unsigned count, unsigned* matchIndex, const UChar* matches, unsigned matchCount)
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
                generateCharacterClassInvertedRange(failures, matchDest, ranges, which, matchIndex, matches, matchCount);
            
            while ((*matchIndex < matchCount) && (matches[*matchIndex] < lo)) {
                matchDest.append(branch32(Equal, character, Imm32((unsigned short)matches[*matchIndex])));
                ++*matchIndex;
            }
            failures.append(jump());

            loOrAbove.link(this);
        } else if (which) {
            Jump loOrAbove = branch32(GreaterThanOrEqual, character, Imm32((unsigned short)lo));

            generateCharacterClassInvertedRange(failures, matchDest, ranges, which, matchIndex, matches, matchCount);
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

void Generator::generateCharacterClassInverted(JumpList& matchDest, const CharacterClass& charClass)
{
    Jump unicodeFail;
    if (charClass.numMatchesUnicode || charClass.numRangesUnicode) {
        Jump isAscii = branch32(LessThanOrEqual, character, Imm32(0x7f));
    
        if (charClass.numMatchesUnicode) {
            for (unsigned i = 0; i < charClass.numMatchesUnicode; ++i) {
                UChar ch = charClass.matchesUnicode[i];
                matchDest.append(branch32(Equal, character, Imm32(ch)));
            }
        }
        
        if (charClass.numRangesUnicode) {
            for (unsigned i = 0; i < charClass.numRangesUnicode; ++i) {
                UChar lo = charClass.rangesUnicode[i].begin;
                UChar hi = charClass.rangesUnicode[i].end;
                
                Jump below = branch32(LessThan, character, Imm32(lo));
                matchDest.append(branch32(LessThanOrEqual, character, Imm32(hi)));
                below.link(this);
            }
        }

        unicodeFail = jump();
        isAscii.link(this);
    }

    if (charClass.numRanges) {
        unsigned matchIndex = 0;
        JumpList failures; 
        generateCharacterClassInvertedRange(failures, matchDest, charClass.ranges, charClass.numRanges, &matchIndex, charClass.matches, charClass.numMatches);
        while (matchIndex < charClass.numMatches)
            matchDest.append(branch32(Equal, character, Imm32((unsigned short)charClass.matches[matchIndex++])));

        failures.link(this);
    } else if (charClass.numMatches) {
        // optimization: gather 'a','A' etc back together, can mask & test once.
        Vector<char> matchesAZaz;

        for (unsigned i = 0; i < charClass.numMatches; ++i) {
            char ch = charClass.matches[i];
            if (m_parser.ignoreCase()) {
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

    if (charClass.numMatchesUnicode || charClass.numRangesUnicode)
        unicodeFail.link(this);
}

void Generator::generateCharacterClass(JumpList& failures, const CharacterClass& charClass, bool invert)
{
    generateLoadCharacter(failures);

    if (invert)
        generateCharacterClassInverted(failures, charClass);
    else {
        JumpList successes;
        generateCharacterClassInverted(successes, charClass);
        failures.append(jump());
        successes.link(this);
    }
    
    add32(Imm32(1), index);
}

void Generator::generateParenthesesAssertion(JumpList& failures)
{
    JumpList disjunctionFailed;

    push(index);
    m_parser.parseDisjunction(disjunctionFailed);
    Jump success = jump();

    disjunctionFailed.link(this);
    pop(index);
    failures.append(jump());

    success.link(this);
    pop(index);
}

void Generator::generateParenthesesInvertedAssertion(JumpList& failures)
{
    JumpList disjunctionFailed;

    push(index);
    m_parser.parseDisjunction(disjunctionFailed);

    // If the disjunction succeeded, the inverted assertion failed.
    pop(index);
    failures.append(jump());

    // If the disjunction failed, the inverted assertion succeeded.
    disjunctionFailed.link(this);
    pop(index);
}

void Generator::generateParenthesesNonGreedy(JumpList& failures, Label start, Jump success, Jump fail)
{
    jump(start);
    success.link(this);
    failures.append(fail);
}

Generator::Jump Generator::generateParenthesesResetTrampoline(JumpList& newFailures, unsigned subpatternIdBefore, unsigned subpatternIdAfter)
{
    Jump skip = jump();
    newFailures.link(this);
    for (unsigned i = subpatternIdBefore + 1; i <= subpatternIdAfter; ++i) {
        store32(Imm32(-1), Address(output, (2 * i) * sizeof(int)));
        store32(Imm32(-1), Address(output, (2 * i + 1) * sizeof(int)));
    }
    
    Jump newFailJump = jump();
    skip.link(this);
    
    return newFailJump;
}

void Generator::generateAssertionBOL(JumpList& failures)
{
    if (m_parser.multiline()) {
        JumpList previousIsNewline;

        // begin of input == success
        previousIsNewline.append(branch32(Equal, index, Imm32(0)));

        // now check prev char against newline characters.
        load16(BaseIndex(input, index, TimesTwo, -2), character);
        generateCharacterClassInverted(previousIsNewline, CharacterClass::newline());

        failures.append(jump());

        previousIsNewline.link(this);
    } else
        failures.append(branch32(NotEqual, index, Imm32(0)));
}

void Generator::generateAssertionEOL(JumpList& failures)
{
    if (m_parser.multiline()) {
        JumpList nextIsNewline;

        generateLoadCharacter(nextIsNewline); // end of input == success
        generateCharacterClassInverted(nextIsNewline, CharacterClass::newline());
        failures.append(jump());
        nextIsNewline.link(this);
    } else {
        failures.append(branch32(NotEqual, length, index));
    }
}

void Generator::generateAssertionWordBoundary(JumpList& failures, bool invert)
{
    JumpList wordBoundary;
    JumpList notWordBoundary;

    // (1) Check if the previous value was a word char

    // (1.1) check for begin of input
    Jump atBegin = branch32(Equal, index, Imm32(0));
    // (1.2) load the last char, and chck if is word character
    load16(BaseIndex(input, index, TimesTwo, -2), character);
    JumpList previousIsWord;
    generateCharacterClassInverted(previousIsWord, CharacterClass::wordchar());
    // (1.3) if we get here, previous is not a word char
    atBegin.link(this);

    // (2) Handle situation where previous was NOT a \w

    generateLoadCharacter(notWordBoundary);
    generateCharacterClassInverted(wordBoundary, CharacterClass::wordchar());
    // (2.1) If we get here, neither chars are word chars
    notWordBoundary.append(jump());

    // (3) Handle situation where previous was a \w

    // (3.0) link success in first match to here
    previousIsWord.link(this);
    generateLoadCharacter(wordBoundary);
    generateCharacterClassInverted(notWordBoundary, CharacterClass::wordchar());
    // (3.1) If we get here, this is an end of a word, within the input.
    
    // (4) Link everything up
    
    if (invert) {
        // handle the fall through case
        wordBoundary.append(jump());
    
        // looking for non word boundaries, so link boundary fails to here.
        notWordBoundary.link(this);

        failures.append(wordBoundary);
    } else {
        // looking for word boundaries, so link successes here.
        wordBoundary.link(this);
        
        failures.append(notWordBoundary);
    }
}

void Generator::generateBackreference(JumpList& failures, unsigned subpatternId)
{
    push(index);
    push(repeatCount);

    // get the start pos of the backref into repeatCount (multipurpose!)
    load32(Address(output, (2 * subpatternId) * sizeof(int)), repeatCount);

    Jump skipIncrement = jump();
    Label topOfLoop(this);

    add32(Imm32(1), index);
    add32(Imm32(1), repeatCount);
    skipIncrement.link(this);

    // check if we're at the end of backref (if we are, success!)
    Jump endOfBackRef = branch32(Equal, Address(output, ((2 * subpatternId) + 1) * sizeof(int)), repeatCount);

    load16(BaseIndex(input, repeatCount, MacroAssembler::TimesTwo), character);

    // check if we've run out of input (this would be a can o'fail)
    Jump endOfInput = branch32(Equal, length, index);

    branch16(Equal, BaseIndex(input, index, TimesTwo), character, topOfLoop);

    endOfInput.link(this);

    // Failure
    pop(repeatCount);
    pop(index);
    failures.append(jump());

    // Success
    endOfBackRef.link(this);
    pop(repeatCount);
    pop();
}

void Generator::terminateAlternative(JumpList& successes, JumpList& failures)
{
    successes.append(jump());
    
    failures.link(this);
    peek(index);
}

void Generator::terminateDisjunction(JumpList& successes)
{
    successes.link(this);
}

} } // namespace JSC::WREC

#endif // ENABLE(WREC)
