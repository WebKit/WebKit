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

#ifndef WRECGenerator_h
#define WRECGenerator_h

#include <wtf/Platform.h>

#if ENABLE(WREC)

#include "Quantifier.h"
#include "X86Assembler.h"
#include <wtf/ASCIICType.h>
#include <wtf/unicode/Unicode.h>

namespace JSC { namespace WREC {

    class CharacterRange;
    class GenerateAtomFunctor;
    class Parser;
    struct CharacterClass;

    class Generator {
    public:
        Generator(Parser& parser, X86Assembler& assembler)
            : m_parser(parser)
            , m_assembler(assembler)
        {
        }

        typedef X86Assembler::JmpSrc JmpSrc;
        typedef X86Assembler::JmpDst JmpDst;
        typedef X86Assembler::RegisterID RegisterID;

        static const RegisterID input = X86::eax;
        static const RegisterID length = X86::ecx;
        static const RegisterID index = X86::edx;
        static const RegisterID character = X86::esi;
        static const RegisterID output = X86::edi;
        static const RegisterID repeatCount = X86::ebx; // How many times the current atom repeats in the current match.
        
        void generateEnter();
        void generateSaveIndex();
        void generateIncrementIndex();
        void generateLoopIfNotEndOfInput(JmpDst);
        void generateReturnSuccess();
        void generateReturnFailure();

        void generateGreedyQuantifier(JmpSrcVector& failures, GenerateAtomFunctor& functor, unsigned min, unsigned max);
        void generateNonGreedyQuantifier(JmpSrcVector& failures, GenerateAtomFunctor& functor, unsigned min, unsigned max);
        void generateBacktrack1();
        void generateBacktrackBackreference(unsigned subpatternId);
        void generateCharacterClass(JmpSrcVector& failures, const CharacterClass& charClass, bool invert);
        void generateCharacterClassInverted(JmpSrcVector& failures, const CharacterClass& charClass);
        void generateCharacterClassInvertedRange(JmpSrcVector& failures, JmpSrcVector& matchDest, const CharacterRange* ranges, unsigned count, unsigned* matchIndex, const UChar* matches, unsigned matchCount);
        void generatePatternCharacter(JmpSrcVector& failures, int ch);
        void generateAssertionWordBoundary(JmpSrcVector& failures, bool invert);
        void generateAssertionBOL(JmpSrcVector& failures);
        void generateAssertionEOL(JmpSrcVector& failures);
        void generateBackreference(JmpSrcVector& failures, unsigned subpatternID);
        void generateBackreferenceQuantifier(JmpSrcVector& failures, Quantifier::Type quantifierType, unsigned subpatternId, unsigned min, unsigned max);
        enum ParenthesesType { capturing, non_capturing, assertion, inverted_assertion }; // order is relied on in generateParentheses()
        JmpSrc generateParentheses(ParenthesesType type);
        JmpSrc generateParenthesesResetTrampoline(JmpSrcVector& newFailures, unsigned subpatternIdBefore, unsigned subpatternIdAfter);
        void generateParenthesesNonGreedy(JmpSrcVector& failures, JmpDst start, JmpSrc success, JmpSrc fail);

        void terminateAlternative(JmpSrcVector& successes, JmpSrcVector& failures);
        void terminateDisjunction(JmpSrcVector& successes);

    private:
        Parser& m_parser;
        X86Assembler& m_assembler;
    };

} } // namespace JSC::WREC

#endif // ENABLE(WREC)

#endif // WRECGenerator_h
