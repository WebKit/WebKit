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

#ifndef Escapes_h
#define Escapes_h

#include <wtf/Platform.h>

#if ENABLE(WREC)

#include <wtf/Assertions.h>

namespace JSC { namespace WREC {

    class CharacterClass;

    class Escape {
    public:
        enum Type {
            PatternCharacter,
            CharacterClass,
            Backreference,
            WordBoundaryAssertion,
            Error,
        };
        
        Escape(Type type)
            : m_type(type)
        {
        }

        Type type() const { return m_type; }

    private:
        Type m_type;
        
    protected:
        // Used by subclasses to store data.
        union {
            int i;
            const WREC::CharacterClass* c;
        } m_u;
        bool m_invert;
    };

    class PatternCharacterEscape : public Escape {
    public:
        static const PatternCharacterEscape& cast(const Escape& escape)
        {
            ASSERT(escape.type() == PatternCharacter);
            return static_cast<const PatternCharacterEscape&>(escape);
        }
        
        PatternCharacterEscape(int character)
            : Escape(PatternCharacter)
        {
            m_u.i = character;
        }
        
        operator Escape() const { return *this; }
        
        int character() const { return m_u.i; }
    };

    class CharacterClassEscape : public Escape {
    public:
        static const CharacterClassEscape& cast(const Escape& escape)
        {
            ASSERT(escape.type() == CharacterClass);
            return static_cast<const CharacterClassEscape&>(escape);
        }
        
        CharacterClassEscape(const WREC::CharacterClass& characterClass, bool invert)
            : Escape(CharacterClass)
        {
            m_u.c = &characterClass;
            m_invert = invert;
        }
        
        operator Escape() { return *this; }
        
        const WREC::CharacterClass& characterClass() const { return *m_u.c; }
        bool invert() const { return m_invert; }
    };

    class BackreferenceEscape : public Escape {
    public:
        static const BackreferenceEscape& cast(const Escape& escape)
        {
            ASSERT(escape.type() == Backreference);
            return static_cast<const BackreferenceEscape&>(escape);
        }
        
        BackreferenceEscape(int subpatternId)
            : Escape(Backreference)
        {
            m_u.i = subpatternId;
        }
        
        operator Escape() const { return *this; }
        
        int subpatternId() const { return m_u.i; }
    };

    class WordBoundaryAssertionEscape : public Escape {
    public:
        static const WordBoundaryAssertionEscape& cast(const Escape& escape)
        {
            ASSERT(escape.type() == WordBoundaryAssertion);
            return static_cast<const WordBoundaryAssertionEscape&>(escape);
        }
        
        WordBoundaryAssertionEscape(bool invert)
            : Escape(WordBoundaryAssertion)
        {
            m_invert = invert;
        }
        
        operator Escape() const { return *this; }
        
        bool invert() const { return m_invert; }
    };

} } // namespace JSC::WREC

#endif // ENABLE(WREC)

#endif // Escapes_h
