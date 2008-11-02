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

#ifndef WREC_h
#define WREC_h

#if ENABLE(WREC)

#include "ustring.h"
#include <masm/X86Assembler.h>
#include <wtf/ASCIICType.h>
#include <wtf/Vector.h>

#if COMPILER(GCC)
#define WREC_CALL __attribute__ ((regparm (3)))
#else
#define WREC_CALL
#endif

namespace JSC {

    typedef int (*WRECFunction)(const UChar* input, unsigned start, unsigned length, int* output) WREC_CALL;

    class GenerateAtomFunctor;
    struct CharacterClassRange;
    struct CharacterClass;

    struct Quantifier {
        enum Type {
            None,
            Greedy,
            NonGreedy,
            Error,
        };

        Quantifier()
            : type(None)
        {
        }

        Quantifier(Type type, unsigned min = 0, unsigned max = noMaxSpecified)
            : type(type)
            , min(min)
            , max(max)
        {
        }

        Type type;

        unsigned min;
        unsigned max;

        static const unsigned noMaxSpecified = UINT_MAX;
    };

    class WRECParser;

    typedef Vector<X86Assembler::JmpSrc> JmpSrcVector;

    class WRECGenerator {
    public:
        WRECGenerator(WRECParser& parser, X86Assembler& jit)
            : m_parser(parser)
            , m_jit(jit)
        {
        }

        typedef X86Assembler::JmpSrc JmpSrc;
        typedef X86Assembler::JmpDst JmpDst;

        // these regs setup by the params
        static const X86Assembler::RegisterID inputRegister = X86::eax;
        static const X86Assembler::RegisterID currentPositionRegister = X86::edx;
        static const X86Assembler::RegisterID lengthRegister = X86::ecx;
        static const X86Assembler::RegisterID currentValueRegister = X86::esi;
        static const X86Assembler::RegisterID outputRegister = X86::edi;
        static const X86Assembler::RegisterID quantifierCountRegister = X86::ebx;

        friend class GenerateAtomFunctor;
        friend class GeneratePatternCharacterFunctor;
        friend class GenerateCharacterClassFunctor;
        friend class GenerateBackreferenceFunctor;
        friend class GenerateParenthesesNonGreedyFunctor;

        void generateGreedyQuantifier(JmpSrcVector& failures, GenerateAtomFunctor& functor, unsigned min, unsigned max);
        void generateNonGreedyQuantifier(JmpSrcVector& failures, GenerateAtomFunctor& functor, unsigned min, unsigned max);
        void generateBacktrack1();
        void generateBacktrackBackreference(unsigned subpatternId);
        void generateCharacterClass(JmpSrcVector& failures, CharacterClass& charClass, bool invert);
        void generateCharacterClassInverted(JmpSrcVector& failures, CharacterClass& charClass);
        void generateCharacterClassInvertedRange(JmpSrcVector& failures, JmpSrcVector& matchDest, const CharacterClassRange* ranges, unsigned count, unsigned* matchIndex, const UChar* matches, unsigned matchCount);
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

        void generateDisjunction(JmpSrcVector& successes, JmpSrcVector& failures);
        void terminateDisjunction(JmpSrcVector& successes);

    private:
        WRECParser& m_parser;
        X86Assembler& m_jit;
    };

    class WRECParser {
    public:
        bool m_ignoreCase;
        bool m_multiline;
        unsigned m_numSubpatterns;
        enum WRECError {
            NoError,
            Error_malformedCharacterClass,
            Error_malformedParentheses,
            Error_malformedPattern,
            Error_malformedQuantifier,
            Error_malformedEscape,
            TempError_unsupportedQuantifier,
            TempError_unsupportedParentheses,
        } m_err;

        WRECParser(const UString& pattern, bool ignoreCase, bool multiline, X86Assembler& jit)
            : m_ignoreCase(ignoreCase)
            , m_multiline(multiline)
            , m_numSubpatterns(0)
            , m_err(NoError)
            , m_generator(*this, jit)
            , m_data(pattern.data())
            , m_size(pattern.size())
            , m_index(0)
        {
        }

        void parseAlternative(JmpSrcVector& failures)
        {
            while (parseTerm(failures)) { }
        }

        void parseDisjunction(JmpSrcVector& failures);

        bool parseTerm(JmpSrcVector& failures);
        bool parseEscape(JmpSrcVector& failures);
        bool parseOctalEscape(JmpSrcVector& failures);
        bool parseParentheses(JmpSrcVector& failures);
        bool parseCharacterClass(JmpSrcVector& failures);
        bool parseCharacterClassQuantifier(JmpSrcVector& failures, CharacterClass& charClass, bool invert);
        bool parsePatternCharacterQualifier(JmpSrcVector& failures, int ch);
        bool parseBackreferenceQuantifier(JmpSrcVector& failures, unsigned subpatternId);

        ALWAYS_INLINE Quantifier parseGreedyQuantifier();
        Quantifier parseQuantifier();

        static const int EndOfPattern = -1;

        int peek()
        {
            if (m_index >= m_size)
                return EndOfPattern;
            return m_data[m_index];
        }

        int consume()
        {
            if (m_index >= m_size)
                return EndOfPattern;
            return m_data[m_index++];
        }

        bool peekIsDigit()
        {
            return WTF::isASCIIDigit(peek());
        }

        unsigned peekDigit()
        {
            ASSERT(peekIsDigit());
            return peek() - '0';
        }

        unsigned consumeDigit()
        {
            ASSERT(peekIsDigit());
            return consume() - '0';
        }

        unsigned consumeNumber()
        {
            int n = consumeDigit();
            while (peekIsDigit()) {
                n *= 10;
                n += consumeDigit();
            }
            return n;
        }

        int consumeHex(int count)
        {
            int n = 0;
            while (count--) {
                if (!WTF::isASCIIHexDigit(peek()))
                    return -1;
                n = (n<<4) | WTF::toASCIIHexValue(consume());
            }
            return n;
        }

        unsigned consumeOctal()
        {
            unsigned n = 0;
            while (n < 32 && WTF::isASCIIOctalDigit(peek()))
                n = n * 8 + (consume() - '0');
            return n;
        }

        bool isEndOfPattern()
        {
            return peek() != EndOfPattern;
        }
        
    private:
        WRECGenerator m_generator;
        const UChar* m_data;
        unsigned m_size;
        unsigned m_index;
    };

} // namespace JSC

#endif // ENABLE(WREC)

#endif // WREC_h
