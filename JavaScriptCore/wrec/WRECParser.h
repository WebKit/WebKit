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

#ifndef Parser_h
#define Parser_h

#include <wtf/Platform.h>

#if ENABLE(WREC)

#include "Escapes.h"
#include "Quantifier.h"
#include "UString.h"
#include "WRECGenerator.h"
#include <wtf/ASCIICType.h>

namespace JSC { namespace WREC {

    struct CharacterClass;

    class Parser {
    typedef Generator::JumpList JumpList;
    typedef Generator::ParenthesesType ParenthesesType;

    friend class SavedState;

    public:
        Parser(const UString& pattern, bool ignoreCase, bool multiline)
            : m_generator(*this)
            , m_data(pattern.data())
            , m_size(pattern.size())
            , m_ignoreCase(ignoreCase)
            , m_multiline(multiline)
        {
            reset();
        }
        
        Generator& generator() { return m_generator; }

        bool ignoreCase() const { return m_ignoreCase; }
        bool multiline() const { return m_multiline; }

        void recordSubpattern() { ++m_numSubpatterns; }
        unsigned numSubpatterns() const { return m_numSubpatterns; }
        
        const char* error() const { return m_error; }
        const char* syntaxError() const { return m_error == ParenthesesNotSupported ? 0 : m_error; }
        
        void parsePattern(JumpList& failures)
        {
            reset();

            parseDisjunction(failures);

            if (peek() != EndOfPattern)
                setError(ParenthesesUnmatched); // Parsing the pattern should fully consume it.
        }

        void parseDisjunction(JumpList& failures);
        void parseAlternative(JumpList& failures);
        bool parseTerm(JumpList& failures);
        bool parseNonCharacterEscape(JumpList& failures, const Escape&);
        bool parseParentheses(JumpList& failures);
        bool parseCharacterClass(JumpList& failures);
        bool parseCharacterClassQuantifier(JumpList& failures, const CharacterClass& charClass, bool invert);
        bool parseBackreferenceQuantifier(JumpList& failures, unsigned subpatternId);

    private:
        class SavedState {
        public:
            SavedState(Parser& parser)
                : m_parser(parser)
                , m_index(parser.m_index)
            {
            }
            
            void restore()
            {
                m_parser.m_index = m_index;
            }

        private:
            Parser& m_parser;
            unsigned m_index;
        };

        void reset()
        {
            m_index = 0;
            m_numSubpatterns = 0;
            m_error = 0;
        }

        void setError(const char* error)
        {
            if (m_error)
                return;
            m_error = error;
        }

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
                n = (n << 4) | WTF::toASCIIHexValue(consume());
            }
            return n;
        }

        unsigned consumeOctal()
        {
            unsigned n = 0;
            while (n < 32 && WTF::isASCIIOctalDigit(peek()))
                n = n * 8 + consumeDigit();
            return n;
        }
        
        ALWAYS_INLINE Quantifier consumeGreedyQuantifier();
        Quantifier consumeQuantifier();
        Escape consumeEscape(bool inCharacterClass);
        ParenthesesType consumeParenthesesType();

        static const int EndOfPattern = -1;

        // Error messages.
        static const char* QuantifierOutOfOrder;
        static const char* QuantifierWithoutAtom;
        static const char* ParenthesesUnmatched;
        static const char* ParenthesesTypeInvalid;
        static const char* ParenthesesNotSupported;
        static const char* CharacterClassUnmatched;
        static const char* CharacterClassOutOfOrder;
        static const char* EscapeUnterminated;

        Generator m_generator;
        const UChar* m_data;
        unsigned m_size;
        unsigned m_index;
        bool m_ignoreCase;
        bool m_multiline;
        unsigned m_numSubpatterns;
        const char* m_error;
    };

} } // namespace JSC::WREC

#endif // ENABLE(WREC)

#endif // Parser_h
