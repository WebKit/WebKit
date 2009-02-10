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
#include "MacroAssembler.h"
#include <wtf/ASCIICType.h>
#include <wtf/unicode/Unicode.h>
#include "WREC.h"

namespace JSC { 

    class JSGlobalData;

    namespace WREC {

    class CharacterRange;
    class GenerateAtomFunctor;
    class Parser;
    struct CharacterClass;

    class Generator : private MacroAssembler {
    public:
        using MacroAssembler::Jump;
        using MacroAssembler::JumpList;
        using MacroAssembler::Label;

        enum ParenthesesType { Capturing, NonCapturing, Assertion, InvertedAssertion, Error };

        static CompiledRegExp compileRegExp(JSGlobalData*, const UString& pattern, unsigned* numSubpatterns_ptr, const char** error_ptr, RefPtr<ExecutablePool>& pool, bool ignoreCase = false, bool multiline = false);
    
        Generator(Parser& parser)
            : m_parser(parser)
        {
        }

#if PLATFORM(X86)
        static const RegisterID input = X86::eax;
        static const RegisterID index = X86::edx;
        static const RegisterID length = X86::ecx;
        static const RegisterID output = X86::edi;

        static const RegisterID character = X86::esi;
        static const RegisterID repeatCount = X86::ebx; // How many times the current atom repeats in the current match.

        static const RegisterID returnRegister = X86::eax;
#endif
#if PLATFORM(X86_64)
        static const RegisterID input = X86::edi;
        static const RegisterID index = X86::esi;
        static const RegisterID length = X86::edx;
        static const RegisterID output = X86::ecx;

        static const RegisterID character = X86::eax;
        static const RegisterID repeatCount = X86::ebx; // How many times the current atom repeats in the current match.

        static const RegisterID returnRegister = X86::eax;
#endif

        void generateEnter();
        void generateSaveIndex();
        void generateIncrementIndex(Jump* failure = 0);
        void generateLoadCharacter(JumpList& failures);
        void generateJumpIfNotEndOfInput(Label);
        void generateReturnSuccess();
        void generateReturnFailure();

        void generateGreedyQuantifier(JumpList& failures, GenerateAtomFunctor& functor, unsigned min, unsigned max);
        void generateNonGreedyQuantifier(JumpList& failures, GenerateAtomFunctor& functor, unsigned min, unsigned max);
        void generateBacktrack1();
        void generateBacktrackBackreference(unsigned subpatternId);
        void generateCharacterClass(JumpList& failures, const CharacterClass& charClass, bool invert);
        void generateCharacterClassInverted(JumpList& failures, const CharacterClass& charClass);
        void generateCharacterClassInvertedRange(JumpList& failures, JumpList& matchDest, const CharacterRange* ranges, unsigned count, unsigned* matchIndex, const UChar* matches, unsigned matchCount);
        void generatePatternCharacter(JumpList& failures, int ch);
        void generatePatternCharacterSequence(JumpList& failures, int* sequence, size_t count);
        void generateAssertionWordBoundary(JumpList& failures, bool invert);
        void generateAssertionBOL(JumpList& failures);
        void generateAssertionEOL(JumpList& failures);
        void generateBackreference(JumpList& failures, unsigned subpatternID);
        void generateBackreferenceQuantifier(JumpList& failures, Quantifier::Type quantifierType, unsigned subpatternId, unsigned min, unsigned max);
        void generateParenthesesAssertion(JumpList& failures);
        void generateParenthesesInvertedAssertion(JumpList& failures);
        Jump generateParenthesesResetTrampoline(JumpList& newFailures, unsigned subpatternIdBefore, unsigned subpatternIdAfter);
        void generateParenthesesNonGreedy(JumpList& failures, Label start, Jump success, Jump fail);

        void terminateAlternative(JumpList& successes, JumpList& failures);
        void terminateDisjunction(JumpList& successes);

    private:
        bool generatePatternCharacterPair(JumpList& failures, int ch1, int ch2);

        Parser& m_parser;
    };

} } // namespace JSC::WREC

#endif // ENABLE(WREC)

#endif // WRECGenerator_h
