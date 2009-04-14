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

#ifndef RegexInterpreter_h
#define RegexInterpreter_h

#include <wtf/Platform.h>

#if ENABLE(YARR)

#include <wtf/unicode/Unicode.h>
#include "RegexParser.h"
#include "RegexPattern.h"

namespace JSC { namespace Yarr {

class ByteDisjunction;

struct ByteTerm {
    enum Type {
        TypeAlternativeBegin,
        TypeAlternativeDisjunction,
        TypeAlternativeEnd,
        TypePatternEnd,
        TypeAssertionBOL,
        TypeAssertionEOL,
        TypeAssertionWordBoundary,
        TypePatternCharacter,
        TypeCharacterClass,
        TypeBackReference,
        TypeParenthesesSubpattern,
        TypeParenthesesSubpatternOnceBegin,
        TypeParenthesesSubpatternOnceEnd,
        TypeParentheticalAssertionOnceBegin,
        TypeParentheticalAssertionOnceEnd,
        TypeCheckInput,
    } type;
    bool invertOrCapture;
    union {
        struct {
            union {
                UChar patternCharacter;
                CharacterClass* characterClass;
                unsigned subpatternId;
            };
            union {
                ByteDisjunction* parenthesesDisjunction;
                unsigned parenthesesWidth;
            };
            QuantifierType quantityType;
            unsigned quantityCount;
        } atom;
        struct {
            bool isParentheses;
            int next;
            int end;
        } alternative;
        unsigned checkInputCount;
    };
    unsigned frameLocation;
    int inputPosition;

    ByteTerm(UChar ch, int inputPos)
        : type(ByteTerm::TypePatternCharacter)
    {
        atom.patternCharacter = ch;
        atom.quantityType = QuantifierFixedCount;
        atom.quantityCount = 1;
        inputPosition = inputPos;
    }

    ByteTerm(CharacterClass* characterClass, bool invert, int inputPos)
        : type(ByteTerm::TypeCharacterClass)
        , invertOrCapture(invert)
    {
        atom.characterClass = characterClass;
        atom.quantityType = QuantifierFixedCount;
        atom.quantityCount = 1;
        inputPosition = inputPos;
    }

    ByteTerm(Type type, unsigned subpatternId, ByteDisjunction* parenthesesInfo, bool invertOrCapture, int inputPos)
        : type(type)
        , invertOrCapture(invertOrCapture)
    {
        atom.subpatternId = subpatternId;
        atom.parenthesesDisjunction = parenthesesInfo;
        atom.quantityType = QuantifierFixedCount;
        atom.quantityCount = 1;
        inputPosition = inputPos;
    }
    
    ByteTerm(Type type, bool invert = false)
        : type(type)
        , invertOrCapture(invert)
    {
        atom.quantityType = QuantifierFixedCount;
        atom.quantityCount = 1;
    }

    ByteTerm(Type type, unsigned subpatternId, bool invertOrCapture, int inputPos)
        : type(type)
        , invertOrCapture(invertOrCapture)
    {
        atom.subpatternId = subpatternId;
        atom.quantityType = QuantifierFixedCount;
        atom.quantityCount = 1;
        inputPosition = inputPos;
    }

    static ByteTerm BOL(int inputPos)
    {
        ByteTerm term(TypeAssertionBOL);
        term.inputPosition = inputPos;
        return term;
    }

    static ByteTerm CheckInput(unsigned count)
    {
        ByteTerm term(TypeCheckInput);
        term.checkInputCount = count;
        return term;
    }

    static ByteTerm EOL(int inputPos)
    {
        ByteTerm term(TypeAssertionEOL);
        term.inputPosition = inputPos;
        return term;
    }

    static ByteTerm WordBoundary(bool invert, int inputPos)
    {
        ByteTerm term(TypeAssertionWordBoundary, invert);
        term.inputPosition = inputPos;
        return term;
    }
    
    static ByteTerm BackReference(unsigned subpatternId, int inputPos)
    {
        return ByteTerm(TypeBackReference, subpatternId, false, inputPos);
    }

    static ByteTerm AlternativeBegin(bool isParentheses)
    {
        ByteTerm term(TypeAlternativeBegin);
        term.alternative.isParentheses = isParentheses;
        term.alternative.next = 0;
        term.alternative.end = 0;
        return term;
    }

    static ByteTerm AlternativeDisjunction(bool isParentheses)
    {
        ByteTerm term(TypeAlternativeDisjunction);
        term.alternative.isParentheses = isParentheses;
        term.alternative.next = 0;
        term.alternative.end = 0;
        return term;
    }

    static ByteTerm AlternativeEnd(bool isParentheses)
    {
        ByteTerm term(TypeAlternativeEnd);
        term.alternative.isParentheses = isParentheses;
        term.alternative.next = 0;
        term.alternative.end = 0;
        return term;
    }

    static ByteTerm PatternEnd()
    {
        return ByteTerm(TypePatternEnd);
    }

    bool invert()
    {
        return invertOrCapture;
    }

    bool capture()
    {
        return invertOrCapture;
    }
};

class ByteDisjunction {
public:
    ByteDisjunction(unsigned numSubpatterns, unsigned frameSize)
        : m_numSubpatterns(numSubpatterns)
        , m_frameSize(frameSize)
    {
    }

    Vector<ByteTerm> terms;
    unsigned m_numSubpatterns;
    unsigned m_frameSize;
};

struct BytecodePattern {
    BytecodePattern(ByteDisjunction* body, Vector<ByteDisjunction*> allParenthesesInfo, RegexPattern& pattern)
        : m_body(body)
        , m_ignoreCase(pattern.m_ignoreCase)
        , m_multiline(pattern.m_multiline)
    {
        newlineCharacterClass = pattern.newlineCharacterClass();
        wordcharCharacterClass = pattern.wordcharCharacterClass();

        m_allParenthesesInfo.append(allParenthesesInfo);
        m_userCharacterClasses.append(pattern.m_userCharacterClasses);
        // 'Steal' the RegexPattern's CharacterClasses!  We clear its
        // array, so that it won't delete them on destruction.  We'll
        // take responsibility for that.
        pattern.m_userCharacterClasses.clear();
    }

    ~BytecodePattern()
    {
        deleteAllValues(m_allParenthesesInfo);
        deleteAllValues(m_userCharacterClasses);
    }

    OwnPtr<ByteDisjunction> m_body;
    bool m_ignoreCase;
    bool m_multiline;
    
    CharacterClass* newlineCharacterClass;
    CharacterClass* wordcharCharacterClass;
private:
    Vector<ByteDisjunction*> m_allParenthesesInfo;
    Vector<CharacterClass*> m_userCharacterClasses;
};

BytecodePattern* byteCompileRegex(const UString& pattern, unsigned& numSubpatterns, const char*& error, bool ignoreCase = false, bool multiline = false);
int interpretRegex(BytecodePattern* v_regex, const UChar* input, unsigned start, unsigned length, int* output);

} } // namespace JSC::Yarr

#endif

#endif // RegexInterpreter_h
