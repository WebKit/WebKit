/*
 * Copyright (C) 2009, 2010-2012, 2014, 2016 Apple Inc. All rights reserved.
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

#pragma once

#include "ConcurrentJSLock.h"
#include "YarrErrorCode.h"
#include "YarrFlags.h"
#include "YarrPattern.h"

namespace WTF {
class BumpPointerAllocator;
}
using WTF::BumpPointerAllocator;

namespace JSC { namespace Yarr {

class ByteDisjunction;

struct ByteTerm {
    union {
        struct {
            union {
                UChar32 patternCharacter;
                struct {
                    UChar32 lo;
                    UChar32 hi;
                } casedCharacter;
                CharacterClass* characterClass;
                unsigned subpatternId;
            };
            union {
                ByteDisjunction* parenthesesDisjunction;
                unsigned parenthesesWidth;
            };
            QuantifierType quantityType;
            unsigned quantityMinCount;
            unsigned quantityMaxCount;
        } atom;
        struct {
            int next;
            int end;
            bool onceThrough;
        } alternative;
        struct {
            bool m_bol : 1;
            bool m_eol : 1;
        } anchors;
        unsigned checkInputCount;
    };
    unsigned frameLocation { 0 };
    enum class Type : uint8_t {
        BodyAlternativeBegin,
        BodyAlternativeDisjunction,
        BodyAlternativeEnd,
        AlternativeBegin,
        AlternativeDisjunction,
        AlternativeEnd,
        SubpatternBegin,
        SubpatternEnd,
        AssertionBOL,
        AssertionEOL,
        AssertionWordBoundary,
        PatternCharacterOnce,
        PatternCharacterFixed,
        PatternCharacterGreedy,
        PatternCharacterNonGreedy,
        PatternCasedCharacterOnce,
        PatternCasedCharacterFixed,
        PatternCasedCharacterGreedy,
        PatternCasedCharacterNonGreedy,
        CharacterClass,
        BackReference,
        ParenthesesSubpattern,
        ParenthesesSubpatternOnceBegin,
        ParenthesesSubpatternOnceEnd,
        ParenthesesSubpatternTerminalBegin,
        ParenthesesSubpatternTerminalEnd,
        ParentheticalAssertionBegin,
        ParentheticalAssertionEnd,
        CheckInput,
        UncheckInput,
        DotStarEnclosure,
    };
    Type type;
    bool m_capture : 1;
    bool m_invert : 1;
    unsigned inputPosition { 0 };

    ByteTerm(UChar32 ch, unsigned inputPos, unsigned frameLocation, Checked<unsigned> quantityCount, QuantifierType quantityType)
        : frameLocation(frameLocation)
        , m_capture(false)
        , m_invert(false)
        , inputPosition(inputPos)
    {
        atom.patternCharacter = ch;
        atom.quantityType = quantityType;
        atom.quantityMinCount = quantityCount;
        atom.quantityMaxCount = quantityCount;

        switch (quantityType) {
        case QuantifierType::FixedCount:
            type = (quantityCount == 1) ? ByteTerm::Type::PatternCharacterOnce : ByteTerm::Type::PatternCharacterFixed;
            break;
        case QuantifierType::Greedy:
            type = ByteTerm::Type::PatternCharacterGreedy;
            break;
        case QuantifierType::NonGreedy:
            type = ByteTerm::Type::PatternCharacterNonGreedy;
            break;
        }
    }

    ByteTerm(UChar32 lo, UChar32 hi, unsigned inputPos, unsigned frameLocation, Checked<unsigned> quantityCount, QuantifierType quantityType)
        : frameLocation(frameLocation)
        , m_capture(false)
        , m_invert(false)
        , inputPosition(inputPos)
    {
        switch (quantityType) {
        case QuantifierType::FixedCount:
            type = (quantityCount == 1) ? ByteTerm::Type::PatternCasedCharacterOnce : ByteTerm::Type::PatternCasedCharacterFixed;
            break;
        case QuantifierType::Greedy:
            type = ByteTerm::Type::PatternCasedCharacterGreedy;
            break;
        case QuantifierType::NonGreedy:
            type = ByteTerm::Type::PatternCasedCharacterNonGreedy;
            break;
        }

        atom.casedCharacter.lo = lo;
        atom.casedCharacter.hi = hi;
        atom.quantityType = quantityType;
        atom.quantityMinCount = quantityCount;
        atom.quantityMaxCount = quantityCount;
    }

    ByteTerm(CharacterClass* characterClass, bool invert, unsigned inputPos)
        : type(ByteTerm::Type::CharacterClass)
        , m_capture(false)
        , m_invert(invert)
        , inputPosition(inputPos)
    {
        atom.characterClass = characterClass;
        atom.quantityType = QuantifierType::FixedCount;
        atom.quantityMinCount = 1;
        atom.quantityMaxCount = 1;
    }

    ByteTerm(Type type, unsigned subpatternId, ByteDisjunction* parenthesesInfo, bool capture, unsigned inputPos)
        : type(type)
        , m_capture(capture)
        , m_invert(false)
        , inputPosition(inputPos)
    {
        atom.subpatternId = subpatternId;
        atom.parenthesesDisjunction = parenthesesInfo;
        atom.quantityType = QuantifierType::FixedCount;
        atom.quantityMinCount = 1;
        atom.quantityMaxCount = 1;
    }
    
    ByteTerm(Type type, bool invert = false)
        : type(type)
        , m_capture(false)
        , m_invert(invert)
    {
        atom.quantityType = QuantifierType::FixedCount;
        atom.quantityMinCount = 1;
        atom.quantityMaxCount = 1;
    }

    ByteTerm(Type type, unsigned subpatternId, bool capture, bool invert, unsigned inputPos)
        : type(type)
        , m_capture(capture)
        , m_invert(invert)
        , inputPosition(inputPos)
    {
        atom.subpatternId = subpatternId;
        atom.quantityType = QuantifierType::FixedCount;
        atom.quantityMinCount = 1;
        atom.quantityMaxCount = 1;
    }

    static ByteTerm BOL(unsigned inputPos)
    {
        ByteTerm term(Type::AssertionBOL);
        term.inputPosition = inputPos;
        return term;
    }

    static ByteTerm CheckInput(Checked<unsigned> count)
    {
        ByteTerm term(Type::CheckInput);
        term.checkInputCount = count;
        return term;
    }

    static ByteTerm UncheckInput(Checked<unsigned> count)
    {
        ByteTerm term(Type::UncheckInput);
        term.checkInputCount = count;
        return term;
    }
    
    static ByteTerm EOL(unsigned inputPos)
    {
        ByteTerm term(Type::AssertionEOL);
        term.inputPosition = inputPos;
        return term;
    }

    static ByteTerm WordBoundary(bool invert, unsigned inputPos)
    {
        ByteTerm term(Type::AssertionWordBoundary, invert);
        term.inputPosition = inputPos;
        return term;
    }
    
    static ByteTerm BackReference(unsigned subpatternId, unsigned inputPos)
    {
        return ByteTerm(Type::BackReference, subpatternId, false, false, inputPos);
    }

    static ByteTerm BodyAlternativeBegin(bool onceThrough)
    {
        ByteTerm term(Type::BodyAlternativeBegin);
        term.alternative.next = 0;
        term.alternative.end = 0;
        term.alternative.onceThrough = onceThrough;
        return term;
    }

    static ByteTerm BodyAlternativeDisjunction(bool onceThrough)
    {
        ByteTerm term(Type::BodyAlternativeDisjunction);
        term.alternative.next = 0;
        term.alternative.end = 0;
        term.alternative.onceThrough = onceThrough;
        return term;
    }

    static ByteTerm BodyAlternativeEnd()
    {
        ByteTerm term(Type::BodyAlternativeEnd);
        term.alternative.next = 0;
        term.alternative.end = 0;
        term.alternative.onceThrough = false;
        return term;
    }

    static ByteTerm AlternativeBegin()
    {
        ByteTerm term(Type::AlternativeBegin);
        term.alternative.next = 0;
        term.alternative.end = 0;
        term.alternative.onceThrough = false;
        return term;
    }

    static ByteTerm AlternativeDisjunction()
    {
        ByteTerm term(Type::AlternativeDisjunction);
        term.alternative.next = 0;
        term.alternative.end = 0;
        term.alternative.onceThrough = false;
        return term;
    }

    static ByteTerm AlternativeEnd()
    {
        ByteTerm term(Type::AlternativeEnd);
        term.alternative.next = 0;
        term.alternative.end = 0;
        term.alternative.onceThrough = false;
        return term;
    }

    static ByteTerm SubpatternBegin()
    {
        return ByteTerm(Type::SubpatternBegin);
    }

    static ByteTerm SubpatternEnd()
    {
        return ByteTerm(Type::SubpatternEnd);
    }
    
    static ByteTerm DotStarEnclosure(bool bolAnchor, bool eolAnchor)
    {
        ByteTerm term(Type::DotStarEnclosure);
        term.anchors.m_bol = bolAnchor;
        term.anchors.m_eol = eolAnchor;
        return term;
    }

    bool invert()
    {
        return m_invert;
    }

    bool capture()
    {
        return m_capture;
    }
};

class ByteDisjunction {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ByteDisjunction(unsigned numSubpatterns, unsigned frameSize)
        : m_numSubpatterns(numSubpatterns)
        , m_frameSize(frameSize)
    {
    }

    size_t estimatedSizeInBytes() const { return terms.capacity() * sizeof(ByteTerm); }

    Vector<ByteTerm> terms;
    unsigned m_numSubpatterns;
    unsigned m_frameSize;
};

struct BytecodePattern {
    WTF_MAKE_FAST_ALLOCATED;
public:
    BytecodePattern(std::unique_ptr<ByteDisjunction> body, Vector<std::unique_ptr<ByteDisjunction>>& parenthesesInfoToAdopt, YarrPattern& pattern, BumpPointerAllocator* allocator, ConcurrentJSLock* lock)
        : m_body(WTFMove(body))
        , m_flags(pattern.m_flags)
        , m_allocator(allocator)
        , m_lock(lock)
    {
        m_body->terms.shrinkToFit();

        newlineCharacterClass = pattern.newlineCharacterClass();
        if (unicode() && ignoreCase())
            wordcharCharacterClass = pattern.wordUnicodeIgnoreCaseCharCharacterClass();
        else
            wordcharCharacterClass = pattern.wordcharCharacterClass();

        m_allParenthesesInfo.swap(parenthesesInfoToAdopt);
        m_allParenthesesInfo.shrinkToFit();

        m_userCharacterClasses.swap(pattern.m_userCharacterClasses);
        m_userCharacterClasses.shrinkToFit();
    }

    size_t estimatedSizeInBytes() const { return m_body->estimatedSizeInBytes(); }
    
    bool ignoreCase() const { return m_flags.contains(Flags::IgnoreCase); }
    bool multiline() const { return m_flags.contains(Flags::Multiline); }
    bool hasIndices() const { return m_flags.contains(Flags::HasIndices); }
    bool sticky() const { return m_flags.contains(Flags::Sticky); }
    bool unicode() const { return m_flags.contains(Flags::Unicode); }
    bool dotAll() const { return m_flags.contains(Flags::DotAll); }

    std::unique_ptr<ByteDisjunction> m_body;
    OptionSet<Flags> m_flags;
    // Each BytecodePattern is associated with a RegExp, each RegExp is associated
    // with a VM.  Cache a pointer to our VM's m_regExpAllocator.
    BumpPointerAllocator* m_allocator;
    ConcurrentJSLock* m_lock;

    CharacterClass* newlineCharacterClass;
    CharacterClass* wordcharCharacterClass;

private:
    Vector<std::unique_ptr<ByteDisjunction>> m_allParenthesesInfo;
    Vector<std::unique_ptr<CharacterClass>> m_userCharacterClasses;
};

JS_EXPORT_PRIVATE std::unique_ptr<BytecodePattern> byteCompile(YarrPattern&, BumpPointerAllocator*, ErrorCode&, ConcurrentJSLock* = nullptr);
JS_EXPORT_PRIVATE unsigned interpret(BytecodePattern*, const String& input, unsigned start, unsigned* output);
unsigned interpret(BytecodePattern*, const LChar* input, unsigned length, unsigned start, unsigned* output);
unsigned interpret(BytecodePattern*, const UChar* input, unsigned length, unsigned start, unsigned* output);

} } // namespace JSC::Yarr
