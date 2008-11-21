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

#define __ m_assembler.

using namespace WTF;

namespace JSC { namespace WREC {

#if COMPILER(MSVC)
// MSVC has 3 extra arguments on the stack because it doesn't use the register calling convention.
static const int outputParameter = 16 + sizeof(const UChar*) + sizeof(unsigned) + sizeof(unsigned);
#else
static const int outputParameter = 16;
#endif

void Generator::generateEnter()
{
    __ convertToFastCall();

    // Save callee save registers.
    __ pushl_r(Generator::output);
    __ pushl_r(Generator::character);
    
    // Initialize output register.
    __ movl_mr(outputParameter, X86::esp, Generator::output);

#ifndef NDEBUG
    // ASSERT that the output register is not null.
    __ testl_rr(Generator::output, Generator::output);
    X86Assembler::JmpSrc outputNotNull = __ jne();
    __ int3();
    __ link(outputNotNull, __ label());
#endif
}

void Generator::generateReturnSuccess()
{
    // Set return value.
    __ popl_r(X86::eax); // match begin
    __ movl_rm(X86::eax, Generator::output);
    __ movl_rm(Generator::index, 4, Generator::output); // match end
    
    // Restore callee save registers.
    __ popl_r(Generator::character);
    __ popl_r(Generator::output);
    __ ret();
}

void Generator::generateSaveIndex()
{
    __ pushl_r(Generator::index);
}

void Generator::generateIncrementIndex()
{
    __ movl_mr(X86::esp, Generator::index);
    __ addl_i8r(1, Generator::index);
    __ movl_rm(Generator::index, X86::esp);
}

void Generator::generateLoadCharacter(JmpSrcVector& failures)
{
    __ cmpl_rr(length, index);
    failures.append(__ je());
    __ movzwl_mr(input, index, 2, character);
}

void Generator::generateLoopIfNotEndOfInput(JmpDst target)
{
    __ cmpl_rr(Generator::length, Generator::index);
    __ link(__ jle(), target);
}

void Generator::generateReturnFailure()
{
    __ addl_i8r(4, X86::esp);
    __ movl_i32r(-1, X86::eax);
    __ popl_r(Generator::character);
    __ popl_r(Generator::output);
    __ ret();
}

void Generator::generateBacktrack1()
{
    __ subl_i8r(1, index);
}

void Generator::generateBacktrackBackreference(unsigned subpatternId)
{
    __ subl_mr((2 * subpatternId + 1) * sizeof(int), output, index);
    __ addl_mr((2 * subpatternId) * sizeof(int), output, index);
}

void Generator::generateBackreferenceQuantifier(JmpSrcVector& failures, Quantifier::Type quantifierType, unsigned subpatternId, unsigned min, unsigned max)
{
    GenerateBackreferenceFunctor functor(subpatternId);

    __ movl_mr((2 * subpatternId) * sizeof(int), output, character);
    __ cmpl_rm(character, ((2 * subpatternId) + 1) * sizeof(int), output);
    JmpSrc skipIfEmpty = __ je();

    ASSERT(quantifierType == Quantifier::Greedy || quantifierType == Quantifier::NonGreedy);
    if (quantifierType == Quantifier::Greedy)
        generateGreedyQuantifier(failures, functor, min, max);
    else
        generateNonGreedyQuantifier(failures, functor, min, max);

    __ link(skipIfEmpty, __ label());
}

void Generator::generateNonGreedyQuantifier(JmpSrcVector& failures, GenerateAtomFunctor& functor, unsigned min, unsigned max)
{
    // comment me better!
    JmpSrcVector newFailures;

    // (0) Setup:
    //     init repeatCount
    __ pushl_r(repeatCount);
    __ xorl_rr(repeatCount, repeatCount);
    JmpSrc gotoStart = __ jmp();

    // (4) Failure case

    JmpDst quantifierFailed = __ label();
    // (4.1) Restore original value of repeatCount from the stack
    __ popl_r(repeatCount);
    failures.append(__ jmp()); 

    // (3) We just tried an alternative, and it failed - check we can try more.
    
    JmpDst alternativeFailed = __ label();
    // (3.1) remove the value pushed prior to testing the alternative
    __ popl_r(index);
    // (3.2) if there is a limit, and we have reached it, game over. 
    if (max != Quantifier::noMaxSpecified) {
        __ cmpl_i32r(max, repeatCount);
        __ link(__ je(), quantifierFailed);
    }

    // (1) Do a check for the atom

    // (1.0) This is where we start, if there is a minimum (then we must read at least one of the atom).
    JmpDst testQuantifiedAtom = __ label();
    if (min)
        __ link(gotoStart, testQuantifiedAtom);
    // (1.1) Do a check for the atom check.
    functor.generateAtom(this, newFailures);
    // (1.2) If we get here, successful match!
    __ addl_i8r(1, repeatCount);
    // (1.3) We needed to read the atom, and we failed - that's terminally  bad news.
    __ link(newFailures, quantifierFailed);
    // (1.4) If there is a minimum, check we have read enough ...
    if (min) {
        // ... except if min was 1 no need to keep checking!
        if (min != 1) {
            __ cmpl_i32r(min, repeatCount);
            __ link(__ jl(), testQuantifiedAtom);
        }
    } else
        __ link(gotoStart, __ label());
    // if there was no minimum, this is where we start.

    // (2) Plant an alternative check for the remainder of the expression
    
    // (2.1) recursively call to parseAlternative, if it falls through, success!
    __ pushl_r(index);
    m_parser.parseAlternative(newFailures);
    __ addl_i8r(4, X86::esp);
    __ popl_r(repeatCount);
    // (2.2) link failure cases to jump back up to alternativeFailed.
    __ link(newFailures, alternativeFailed);
}

void Generator::generateGreedyQuantifier(JmpSrcVector& failures, GenerateAtomFunctor& functor, unsigned min, unsigned max)
{
    if (!max)
        return;

    // comment me better!
    JmpSrcVector newFailures;

    // (0) Setup:
    //     init repeatCount
    __ pushl_r(repeatCount);
    __ xorl_rr(repeatCount, repeatCount);

    // (1) Greedily read as many of the atom as possible

    JmpDst readMore = __ label();

    // (1.1) Do a character class check.
    functor.generateAtom(this, newFailures);
    // (1.2) If we get here, successful match!
    __ addl_i8r(1, repeatCount);
    // (1.3) loop, unless we've read max limit.
    if (max != Quantifier::noMaxSpecified) {
        if (max != 1) {
            // if there is a limit, only loop while less than limit, otherwise fall throught to...
            __ cmpl_i32r(max, repeatCount);
            __ link(__ jne(), readMore);
        }
        // ...if there is no min we need jump to the alternative test, if there is we can just fall through to it.
        if (!min)
            newFailures.append(__ jmp());
    } else
        __ link(__ jmp(), readMore);
    // (1.4) check enough matches to bother trying an alternative...
    if (min) {
        // We will fall through to here if (min && max), after the max check.
        // First, also link a
        __ link(newFailures, __ label());
        __ cmpl_i32r(min, repeatCount);
        newFailures.append(__ jae());
    }

    // (4) Failure case

    JmpDst quantifierFailed = __ label();
    // (4.1) Restore original value of repeatCount from the stack
    __ popl_r(repeatCount);
    failures.append(__ jmp()); 

    // (3) Backtrack

    JmpDst backtrack = __ label();
    // (3.1) this was preserved prior to executing the alternative
    __ popl_r(index);
    // (3.2) check we can retry with fewer matches - backtracking fails if already at the minimum
    __ cmpl_i32r(min, repeatCount);
    __ link(__ je(), quantifierFailed);
    // (3.3) roll off one match, and retry.
    functor.backtrack(this);
    __ subl_i8r(1, repeatCount);

    // (2) Try an alternative.

    // (2.1) point to retry
    __ link(newFailures, __ label());
    // (2.2) recursively call to parseAlternative, if it falls through, success!
    __ pushl_r(index);
    m_parser.parseAlternative(newFailures);
    __ addl_i8r(4, X86::esp);
    __ popl_r(repeatCount);
    // (2.3) link failure cases to here.
    __ link(newFailures, backtrack);
}

void Generator::generatePatternCharacter(JmpSrcVector& failures, int ch)
{
    generateLoadCharacter(failures);

    // used for unicode case insensitive
    bool hasUpper = false;
    JmpSrc isUpper;
    
    // if case insensitive match
    if (m_parser.ignoreCase()) {
        UChar lower, upper;
        
        // check for ascii case sensitive characters
        if (isASCIIAlpha(ch)) {
            __ orl_i32r(32, character);
            ch |= 32;
        } else if ((ch > 0x7f) && ((lower = Unicode::toLower(ch)) != (upper = Unicode::toUpper(ch)))) {
            // handle unicode case sentitive characters - branch to success on upper
            __ cmpl_i32r(upper, character);
            isUpper = __ je();
            hasUpper = true;
            ch = lower;
        }
    }
    
    // checks for ch, or lower case version of ch, if insensitive
    __ cmpl_i32r((unsigned short)ch, character);
    failures.append(__ jne());

    if (m_parser.ignoreCase() && hasUpper) {
        // for unicode case insensitive matches, branch here if upper matches.
        __ link(isUpper, __ label());
    }
    
    // on success consume the char
    __ addl_i8r(1, index);
}

void Generator::generateCharacterClassInvertedRange(JmpSrcVector& failures, JmpSrcVector& matchDest, const CharacterRange* ranges, unsigned count, unsigned* matchIndex, const UChar* matches, unsigned matchCount)
{
    do {
        // pick which range we're going to generate
        int which = count >> 1;
        char lo = ranges[which].begin;
        char hi = ranges[which].end;
        
        __ cmpl_i32r((unsigned short)lo, character);

        // check if there are any ranges or matches below lo.  If not, just jl to failure -
        // if there is anything else to check, check that first, if it falls through jmp to failure.
        if ((*matchIndex < matchCount) && (matches[*matchIndex] < lo)) {
            JmpSrc loOrAbove = __ jge();
            
            // generate code for all ranges before this one
            if (which)
                generateCharacterClassInvertedRange(failures, matchDest, ranges, which, matchIndex, matches, matchCount);
            
            do {
                __ cmpl_i32r((unsigned short)matches[*matchIndex], character);
                matchDest.append(__ je());
                ++*matchIndex;
            } while ((*matchIndex < matchCount) && (matches[*matchIndex] < lo));
            failures.append(__ jmp());

            __ link(loOrAbove, __ label());
        } else if (which) {
            JmpSrc loOrAbove = __ jge();

            generateCharacterClassInvertedRange(failures, matchDest, ranges, which, matchIndex, matches, matchCount);
            failures.append(__ jmp());

            __ link(loOrAbove, __ label());
        } else
            failures.append(__ jl());

        while ((*matchIndex < matchCount) && (matches[*matchIndex] <= hi))
            ++*matchIndex;

        __ cmpl_i32r((unsigned short)hi, character);
        matchDest.append(__ jle());
        // fall through to here, the value is above hi.

        // shuffle along & loop around if there are any more matches to handle.
        unsigned next = which + 1;
        ranges += next;
        count -= next;
    } while (count);
}

void Generator::generateCharacterClassInverted(JmpSrcVector& matchDest, const CharacterClass& charClass)
{
    JmpSrc unicodeFail;
    if (charClass.numMatchesUnicode || charClass.numRangesUnicode) {
        __ cmpl_i8r(0x7f, character);
        JmpSrc isAscii = __ jle();
    
        if (charClass.numMatchesUnicode) {
            for (unsigned i = 0; i < charClass.numMatchesUnicode; ++i) {
                UChar ch = charClass.matchesUnicode[i];
                __ cmpl_i32r((unsigned short)ch, character);
                matchDest.append(__ je());
            }
        }
        
        if (charClass.numRangesUnicode) {
            for (unsigned i = 0; i < charClass.numRangesUnicode; ++i) {
                UChar lo = charClass.rangesUnicode[i].begin;
                UChar hi = charClass.rangesUnicode[i].end;
                
                __ cmpl_i32r((unsigned short)lo, character);
                JmpSrc below = __ jl();
                __ cmpl_i32r((unsigned short)hi, character);
                matchDest.append(__ jle());
                __ link(below, __ label());
            }
        }

        unicodeFail = __ jmp();
        __ link(isAscii, __ label());
    }

    if (charClass.numRanges) {
        unsigned matchIndex = 0;
        JmpSrcVector failures; 
        generateCharacterClassInvertedRange(failures, matchDest, charClass.ranges, charClass.numRanges, &matchIndex, charClass.matches, charClass.numMatches);
        while (matchIndex < charClass.numMatches) {
            __ cmpl_i32r((unsigned short)charClass.matches[matchIndex], character);
            matchDest.append(__ je());
            ++matchIndex;
        }
        __ link(failures, __ label());
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
            __ cmpl_i32r((unsigned short)ch, character);
            matchDest.append(__ je());
        }

        if (unsigned countAZaz = matchesAZaz.size()) {
            __ orl_i32r(32, character);

            for (unsigned i = 0; i < countAZaz; ++i) {
                char ch = matchesAZaz[i];
                __ cmpl_i32r((unsigned short)ch, character);
                matchDest.append(__ je());
            }
        }
    }

    if (charClass.numMatchesUnicode || charClass.numRangesUnicode)
        __ link(unicodeFail, __ label());
}

void Generator::generateCharacterClass(JmpSrcVector& failures, const CharacterClass& charClass, bool invert)
{
    generateLoadCharacter(failures);

    if (invert)
        generateCharacterClassInverted(failures, charClass);
    else {
        JmpSrcVector successes;
        generateCharacterClassInverted(successes, charClass);
        failures.append(__ jmp());
        __ link(successes, __ label());
    }
    
    __ addl_i8r(1, index);
}

Generator::JmpSrc Generator::generateParentheses(ParenthesesType type)
{
    if (type == capturing)
        m_parser.recordSubpattern();

    unsigned subpatternId = m_parser.numSubpatterns();

    // push pos, both to preserve for fail + reloaded in parseDisjunction
    __ pushl_r(index);

    // Plant a disjunction, wrapped to invert behaviour - 
    JmpSrcVector newFailures;
    m_parser.parseDisjunction(newFailures);
    
    if (type == capturing) {
        __ popl_r(character);
        __ movl_rm(character, (2 * subpatternId) * sizeof(int), output);
        __ movl_rm(index, (2 * subpatternId + 1) * sizeof(int), output);
    } else if (type == non_capturing)
        __ addl_i8r(4, X86::esp);
    else
        __ popl_r(index);

    // This is a little lame - jump to jump if there is a nested disjunction.
    // (suggestion to fix: make parseDisjunction populate a JmpSrcVector of
    // disjunct successes... this is probably not worth the compile cost in
    // the common case to fix).
    JmpSrc successfulMatch = __ jmp();

    __ link(newFailures, __ label());
    __ popl_r(index);

    JmpSrc jumpToFail;
    // If this was an inverted assert, fail = success! - just let the failure case drop through,
    // success case goes to failures.  Both paths restore curr pos.
    if (type == inverted_assertion)
        jumpToFail = successfulMatch;
    else {
        // plant a jump so any fail will link off to 'failures',
        jumpToFail = __ jmp();
        // link successes to jump here
        __ link(successfulMatch, __ label());
    }
    return jumpToFail;
}

void Generator::generateParenthesesNonGreedy(JmpSrcVector& failures, JmpDst start, JmpSrc success, JmpSrc fail)
{
    __ link(__ jmp(), start);
    __ link(success, __ label());

    failures.append(fail);
}

Generator::JmpSrc Generator::generateParenthesesResetTrampoline(JmpSrcVector& newFailures, unsigned subpatternIdBefore, unsigned subpatternIdAfter)
{
    JmpSrc skip = __ jmp();
    __ link(newFailures, __ label());
    for (unsigned i = subpatternIdBefore + 1; i <= subpatternIdAfter; ++i) {
        __ movl_i32m(-1, (2 * i) * sizeof(int), output);
        __ movl_i32m(-1, (2 * i + 1) * sizeof(int), output);
    }
    
    JmpSrc newFailJump = __ jmp();
    __ link(skip, __ label());
    
    return newFailJump;
}

void Generator::generateAssertionBOL(JmpSrcVector& failures)
{
    if (m_parser.multiline()) {
        JmpSrcVector previousIsNewline;

        // begin of input == success
        __ cmpl_i8r(0, index);
        previousIsNewline.append(__ je());

        // now check prev char against newline characters.
        __ movzwl_mr(-2, input, index, 2, character);
        generateCharacterClassInverted(previousIsNewline, CharacterClass::newline());

        failures.append(__ jmp());

        __ link(previousIsNewline, __ label());
    } else {
        __ cmpl_i8r(0, index);
        failures.append(__ jne());
    }
}

void Generator::generateAssertionEOL(JmpSrcVector& failures)
{
    if (m_parser.multiline()) {
        JmpSrcVector nextIsNewline;

        generateLoadCharacter(nextIsNewline); // end of input == success
        generateCharacterClassInverted(nextIsNewline, CharacterClass::newline());
        failures.append(__ jmp());
        __ link(nextIsNewline, __ label());
    } else {
        __ cmpl_rr(length, index);
        failures.append(__ jne());
    }
}

void Generator::generateAssertionWordBoundary(JmpSrcVector& failures, bool invert)
{
    JmpSrcVector wordBoundary;
    JmpSrcVector notWordBoundary;

    // (1) Check if the previous value was a word char

    // (1.1) check for begin of input
    __ cmpl_i8r(0, index);
    JmpSrc atBegin = __ je();
    // (1.2) load the last char, and chck if is word character
    __ movzwl_mr(-2, input, index, 2, character);
    JmpSrcVector previousIsWord;
    generateCharacterClassInverted(previousIsWord, CharacterClass::wordchar());
    // (1.3) if we get here, previous is not a word char
    __ link(atBegin, __ label());

    // (2) Handle situation where previous was NOT a \w

    generateLoadCharacter(notWordBoundary);
    generateCharacterClassInverted(wordBoundary, CharacterClass::wordchar());
    // (2.1) If we get here, neither chars are word chars
    notWordBoundary.append(__ jmp());

    // (3) Handle situation where previous was a \w

    // (3.0) link success in first match to here
    __ link(previousIsWord, __ label());
    generateLoadCharacter(wordBoundary);
    generateCharacterClassInverted(notWordBoundary, CharacterClass::wordchar());
    // (3.1) If we get here, this is an end of a word, within the input.
    
    // (4) Link everything up
    
    if (invert) {
        // handle the fall through case
        wordBoundary.append(__ jmp());
    
        // looking for non word boundaries, so link boundary fails to here.
        __ link(notWordBoundary, __ label());

        failures.append(wordBoundary.begin(), wordBoundary.size());
    } else {
        // looking for word boundaries, so link successes here.
        __ link(wordBoundary, __ label());
        
        failures.append(notWordBoundary.begin(), notWordBoundary.size());
    }
}

void Generator::generateBackreference(JmpSrcVector& failures, unsigned subpatternId)
{
    __ pushl_r(index);
    __ pushl_r(repeatCount);

    // get the start pos of the backref into repeatCount (multipurpose!)
    __ movl_mr((2 * subpatternId) * sizeof(int), output, repeatCount);

    JmpSrc skipIncrement = __ jmp();
    JmpDst topOfLoop = __ label();
    __ addl_i8r(1, index);
    __ addl_i8r(1, repeatCount);
    __ link(skipIncrement, __ label());

    // check if we're at the end of backref (if we are, success!)
    __ cmpl_rm(repeatCount, ((2 * subpatternId) + 1) * sizeof(int), output);
    JmpSrc endOfBackRef = __ je();
    
    __ movzwl_mr(input, repeatCount, 2, character);
    
    // check if we've run out of input (this would be a can o'fail)
    __ cmpl_rr(length, index);
    JmpSrc endOfInput = __ je();
    
    __ cmpw_rm(character, input, index, 2);
    __ link(__ je(), topOfLoop);
    
    __ link(endOfInput, __ label());
    // Failure
    __ popl_r(repeatCount);
    __ popl_r(index);
    failures.append(__ jmp());
    
    // Success
    __ link(endOfBackRef, __ label());
    __ popl_r(repeatCount);
    __ addl_i8r(4, X86::esp);
}

void Generator::terminateAlternative(JmpSrcVector& successes, JmpSrcVector& failures)
{
    successes.append(__ jmp());
    
    __ link(failures, __ label());
    __ movl_mr(X86::esp, index);
}

void Generator::terminateDisjunction(JmpSrcVector& successes)
{
    __ link(successes, __ label());
}

} } // namespace JSC::WREC

#endif // ENABLE(WREC)
