/*
 * Copyright (C) 2009, 2013-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Peter Varga (pvarga@inf.u-szeged.hu), University of Szeged
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

#include "YarrErrorCode.h"
#include "YarrFlags.h"
#include "YarrUnicodeProperties.h"
#include <wtf/CheckedArithmetic.h>
#include <wtf/HashMap.h>
#include <wtf/OptionSet.h>
#include <wtf/PrintStream.h>
#include <wtf/Vector.h>
#include <wtf/text/StringHash.h>

namespace JSC { namespace Yarr {

struct YarrPattern;
struct PatternDisjunction;

struct CharacterRange {
    UChar32 begin { 0 };
    UChar32 end { 0x10ffff };

    CharacterRange(UChar32 begin, UChar32 end)
        : begin(begin)
        , end(end)
    {
    }
};

enum struct CharacterClassWidths : unsigned char {
    Unknown = 0x0,
    HasBMPChars = 0x1,
    HasNonBMPChars = 0x2,
    HasBothBMPAndNonBMP = HasBMPChars | HasNonBMPChars
};

inline CharacterClassWidths operator|(CharacterClassWidths lhs, CharacterClassWidths rhs)
{
    return static_cast<CharacterClassWidths>(static_cast<unsigned>(lhs) | static_cast<unsigned>(rhs));
}

inline bool operator&(CharacterClassWidths lhs, CharacterClassWidths rhs)
{
    return static_cast<unsigned>(lhs) & static_cast<unsigned>(rhs);
}

inline CharacterClassWidths& operator|=(CharacterClassWidths& lhs, CharacterClassWidths rhs)
{
    lhs = lhs | rhs;
    return lhs;
}

struct CharacterClass {
    WTF_MAKE_FAST_ALLOCATED;
public:
    // All CharacterClass instances have to have the full set of matches and ranges,
    // they may have an optional m_table for faster lookups (which must match the
    // specified matches and ranges)
    CharacterClass()
        : m_table(nullptr)
        , m_characterWidths(CharacterClassWidths::Unknown)
        , m_anyCharacter(false)
    {
    }
    CharacterClass(const char* table, bool inverted)
        : m_table(table)
        , m_characterWidths(CharacterClassWidths::Unknown)
        , m_tableInverted(inverted)
        , m_anyCharacter(false)
    {
    }
    CharacterClass(std::initializer_list<UChar32> matches, std::initializer_list<CharacterRange> ranges, std::initializer_list<UChar32> matchesUnicode, std::initializer_list<CharacterRange> rangesUnicode, CharacterClassWidths widths)
        : m_matches(matches)
        , m_ranges(ranges)
        , m_matchesUnicode(matchesUnicode)
        , m_rangesUnicode(rangesUnicode)
        , m_table(nullptr)
        , m_characterWidths(widths)
        , m_tableInverted(false)
        , m_anyCharacter(false)
    {
    }

    bool hasNonBMPCharacters() { return m_characterWidths & CharacterClassWidths::HasNonBMPChars; }

    bool hasOneCharacterSize() { return m_characterWidths == CharacterClassWidths::HasBMPChars || m_characterWidths == CharacterClassWidths::HasNonBMPChars; }
    bool hasOnlyNonBMPCharacters() { return m_characterWidths == CharacterClassWidths::HasNonBMPChars; }
    
    Vector<UChar32> m_matches;
    Vector<CharacterRange> m_ranges;
    Vector<UChar32> m_matchesUnicode;
    Vector<CharacterRange> m_rangesUnicode;

    const char* m_table;
    CharacterClassWidths m_characterWidths;
    bool m_tableInverted : 1;
    bool m_anyCharacter : 1;
};

enum class QuantifierType : uint8_t {
    FixedCount,
    Greedy,
    NonGreedy,
};

struct PatternTerm {
    enum class Type : uint8_t {
        AssertionBOL,
        AssertionEOL,
        AssertionWordBoundary,
        PatternCharacter,
        CharacterClass,
        BackReference,
        ForwardReference,
        ParenthesesSubpattern,
        ParentheticalAssertion,
        DotStarEnclosure,
    };
    Type type;
    bool m_capture :1;
    bool m_invert :1;
    QuantifierType quantityType;
    Checked<unsigned> quantityMinCount;
    Checked<unsigned> quantityMaxCount;
    union {
        UChar32 patternCharacter;
        CharacterClass* characterClass;
        unsigned backReferenceSubpatternId;
        struct {
            PatternDisjunction* disjunction;
            unsigned subpatternId;
            unsigned lastSubpatternId;
            bool isCopy;
            bool isTerminal;
        } parentheses;
        struct {
            bool bolAnchor : 1;
            bool eolAnchor : 1;
        } anchors;
    };
    unsigned inputPosition;
    unsigned frameLocation;

    PatternTerm(UChar32 ch)
        : type(PatternTerm::Type::PatternCharacter)
        , m_capture(false)
        , m_invert(false)
    {
        patternCharacter = ch;
        quantityType = QuantifierType::FixedCount;
        quantityMinCount = quantityMaxCount = 1;
    }

    PatternTerm(CharacterClass* charClass, bool invert)
        : type(PatternTerm::Type::CharacterClass)
        , m_capture(false)
        , m_invert(invert)
    {
        characterClass = charClass;
        quantityType = QuantifierType::FixedCount;
        quantityMinCount = quantityMaxCount = 1;
    }

    PatternTerm(Type type, unsigned subpatternId, PatternDisjunction* disjunction, bool capture = false, bool invert = false)
        : type(type)
        , m_capture(capture)
        , m_invert(invert)
    {
        parentheses.disjunction = disjunction;
        parentheses.subpatternId = subpatternId;
        parentheses.isCopy = false;
        parentheses.isTerminal = false;
        quantityType = QuantifierType::FixedCount;
        quantityMinCount = quantityMaxCount = 1;
    }
    
    PatternTerm(Type type, bool invert = false)
        : type(type)
        , m_capture(false)
        , m_invert(invert)
    {
        quantityType = QuantifierType::FixedCount;
        quantityMinCount = quantityMaxCount = 1;
    }

    PatternTerm(unsigned spatternId)
        : type(Type::BackReference)
        , m_capture(false)
        , m_invert(false)
    {
        backReferenceSubpatternId = spatternId;
        quantityType = QuantifierType::FixedCount;
        quantityMinCount = quantityMaxCount = 1;
    }

    PatternTerm(bool bolAnchor, bool eolAnchor)
        : type(Type::DotStarEnclosure)
        , m_capture(false)
        , m_invert(false)
    {
        anchors.bolAnchor = bolAnchor;
        anchors.eolAnchor = eolAnchor;
        quantityType = QuantifierType::FixedCount;
        quantityMinCount = quantityMaxCount = 1;
    }
    
    static PatternTerm ForwardReference()
    {
        return PatternTerm(Type::ForwardReference);
    }

    static PatternTerm BOL()
    {
        return PatternTerm(Type::AssertionBOL);
    }

    static PatternTerm EOL()
    {
        return PatternTerm(Type::AssertionEOL);
    }

    static PatternTerm WordBoundary(bool invert)
    {
        return PatternTerm(Type::AssertionWordBoundary, invert);
    }
    
    bool invert() const
    {
        return m_invert;
    }

    bool capture()
    {
        return m_capture;
    }

    bool isFixedWidthCharacterClass() const
    {
        return type == Type::CharacterClass && characterClass->hasOneCharacterSize() && !invert();
    }

    bool containsAnyCaptures()
    {
        ASSERT(this->type == Type::ParenthesesSubpattern);
        return parentheses.lastSubpatternId >= parentheses.subpatternId;
    }

    void quantify(unsigned count, QuantifierType type)
    {
        quantityMinCount = 0;
        quantityMaxCount = count;
        quantityType = type;
    }

    void quantify(unsigned minCount, unsigned maxCount, QuantifierType type)
    {
        // Currently only Parentheses can specify a non-zero min with a different max.
        ASSERT(this->type == Type::ParenthesesSubpattern || !minCount || minCount == maxCount);
        ASSERT(minCount <= maxCount);
        quantityMinCount = minCount;
        quantityMaxCount = maxCount;
        quantityType = type;
    }

    void dumpQuantifier(PrintStream&);
    void dump(PrintStream&, YarrPattern*, unsigned);
};

struct PatternAlternative {
    WTF_MAKE_FAST_ALLOCATED;
public:
    PatternAlternative(PatternDisjunction* disjunction)
        : m_parent(disjunction)
        , m_onceThrough(false)
        , m_hasFixedSize(false)
        , m_startsWithBOL(false)
        , m_containsBOL(false)
    {
    }

    PatternTerm& lastTerm()
    {
        ASSERT(m_terms.size());
        return m_terms[m_terms.size() - 1];
    }
    
    void removeLastTerm()
    {
        ASSERT(m_terms.size());
        m_terms.shrink(m_terms.size() - 1);
    }
    
    void setOnceThrough()
    {
        m_onceThrough = true;
    }
    
    bool onceThrough()
    {
        return m_onceThrough;
    }

    void dump(PrintStream&, YarrPattern*, unsigned);

    Vector<PatternTerm> m_terms;
    PatternDisjunction* m_parent;
    unsigned m_minimumSize;
    bool m_onceThrough : 1;
    bool m_hasFixedSize : 1;
    bool m_startsWithBOL : 1;
    bool m_containsBOL : 1;
};

struct PatternDisjunction {
    WTF_MAKE_FAST_ALLOCATED;
public:
    PatternDisjunction(PatternAlternative* parent = nullptr)
        : m_parent(parent)
        , m_hasFixedSize(false)
    {
    }
    
    PatternAlternative* addNewAlternative()
    {
        m_alternatives.append(makeUnique<PatternAlternative>(this));
        return static_cast<PatternAlternative*>(m_alternatives.last().get());
    }

    void dump(PrintStream&, YarrPattern*, unsigned);

    Vector<std::unique_ptr<PatternAlternative>> m_alternatives;
    PatternAlternative* m_parent;
    unsigned m_minimumSize;
    unsigned m_callFrameSize;
    bool m_hasFixedSize;
};

// You probably don't want to be calling these functions directly
// (please to be calling newlineCharacterClass() et al on your
// friendly neighborhood YarrPattern instance to get nicely
// cached copies).

std::unique_ptr<CharacterClass> anycharCreate();
std::unique_ptr<CharacterClass> newlineCreate();
std::unique_ptr<CharacterClass> digitsCreate();
std::unique_ptr<CharacterClass> spacesCreate();
std::unique_ptr<CharacterClass> wordcharCreate();
std::unique_ptr<CharacterClass> wordUnicodeIgnoreCaseCharCreate();
std::unique_ptr<CharacterClass> nondigitsCreate();
std::unique_ptr<CharacterClass> nonspacesCreate();
std::unique_ptr<CharacterClass> nonwordcharCreate();
std::unique_ptr<CharacterClass> nonwordUnicodeIgnoreCaseCharCreate();

struct TermChain {
    TermChain(PatternTerm term)
        : term(term)
    {}

    PatternTerm term;
    Vector<TermChain> hotTerms;
};


struct YarrPattern {
    JS_EXPORT_PRIVATE YarrPattern(const String& pattern, OptionSet<Flags>, ErrorCode&);

    void resetForReparsing()
    {
        m_numSubpatterns = 0;
        m_initialStartValueFrameLocation = 0;

        m_containsBackreferences = false;
        m_containsBOL = false;
        m_containsUnsignedLengthPattern = false;
        m_hasCopiedParenSubexpressions = false;
        m_saveInitialStartValue = false;

        anycharCached = nullptr;
        newlineCached = nullptr;
        digitsCached = nullptr;
        spacesCached = nullptr;
        wordcharCached = nullptr;
        wordUnicodeIgnoreCaseCharCached = nullptr;
        nondigitsCached = nullptr;
        nonspacesCached = nullptr;
        nonwordcharCached = nullptr;
        nonwordUnicodeIgnoreCasecharCached = nullptr;
        unicodePropertiesCached.clear();

        m_disjunctions.clear();
        m_userCharacterClasses.clear();
        m_captureGroupNames.clear();
    }

    bool containsUnsignedLengthPattern()
    {
        return m_containsUnsignedLengthPattern;
    }

    CharacterClass* anyCharacterClass()
    {
        if (!anycharCached) {
            m_userCharacterClasses.append(anycharCreate());
            anycharCached = m_userCharacterClasses.last().get();
        }
        return anycharCached;
    }
    CharacterClass* newlineCharacterClass()
    {
        if (!newlineCached) {
            m_userCharacterClasses.append(newlineCreate());
            newlineCached = m_userCharacterClasses.last().get();
        }
        return newlineCached;
    }
    CharacterClass* digitsCharacterClass()
    {
        if (!digitsCached) {
            m_userCharacterClasses.append(digitsCreate());
            digitsCached = m_userCharacterClasses.last().get();
        }
        return digitsCached;
    }
    CharacterClass* spacesCharacterClass()
    {
        if (!spacesCached) {
            m_userCharacterClasses.append(spacesCreate());
            spacesCached = m_userCharacterClasses.last().get();
        }
        return spacesCached;
    }
    CharacterClass* wordcharCharacterClass()
    {
        if (!wordcharCached) {
            m_userCharacterClasses.append(wordcharCreate());
            wordcharCached = m_userCharacterClasses.last().get();
        }
        return wordcharCached;
    }
    CharacterClass* wordUnicodeIgnoreCaseCharCharacterClass()
    {
        if (!wordUnicodeIgnoreCaseCharCached) {
            m_userCharacterClasses.append(wordUnicodeIgnoreCaseCharCreate());
            wordUnicodeIgnoreCaseCharCached = m_userCharacterClasses.last().get();
        }
        return wordUnicodeIgnoreCaseCharCached;
    }
    CharacterClass* nondigitsCharacterClass()
    {
        if (!nondigitsCached) {
            m_userCharacterClasses.append(nondigitsCreate());
            nondigitsCached = m_userCharacterClasses.last().get();
        }
        return nondigitsCached;
    }
    CharacterClass* nonspacesCharacterClass()
    {
        if (!nonspacesCached) {
            m_userCharacterClasses.append(nonspacesCreate());
            nonspacesCached = m_userCharacterClasses.last().get();
        }
        return nonspacesCached;
    }
    CharacterClass* nonwordcharCharacterClass()
    {
        if (!nonwordcharCached) {
            m_userCharacterClasses.append(nonwordcharCreate());
            nonwordcharCached = m_userCharacterClasses.last().get();
        }
        return nonwordcharCached;
    }
    CharacterClass* nonwordUnicodeIgnoreCaseCharCharacterClass()
    {
        if (!nonwordUnicodeIgnoreCasecharCached) {
            m_userCharacterClasses.append(nonwordUnicodeIgnoreCaseCharCreate());
            nonwordUnicodeIgnoreCasecharCached = m_userCharacterClasses.last().get();
        }
        return nonwordUnicodeIgnoreCasecharCached;
    }
    CharacterClass* unicodeCharacterClassFor(BuiltInCharacterClassID unicodeClassID)
    {
        ASSERT(unicodeClassID >= BuiltInCharacterClassID::BaseUnicodePropertyID);

        unsigned classID = static_cast<unsigned>(unicodeClassID);

        if (unicodePropertiesCached.find(classID) == unicodePropertiesCached.end()) {
            m_userCharacterClasses.append(createUnicodeCharacterClassFor(unicodeClassID));
            CharacterClass* result = m_userCharacterClasses.last().get();
            unicodePropertiesCached.add(classID, result);
            return result;
        }

        return unicodePropertiesCached.get(classID);
    }

    void dumpPatternString(PrintStream& out, const String& patternString);
    void dumpPattern(const String& pattern);
    void dumpPattern(PrintStream& out, const String& pattern);

    bool global() const { return m_flags.contains(Flags::Global); }
    bool ignoreCase() const { return m_flags.contains(Flags::IgnoreCase); }
    bool multiline() const { return m_flags.contains(Flags::Multiline); }
    bool hasIndices() const { return m_flags.contains(Flags::HasIndices); }
    bool sticky() const { return m_flags.contains(Flags::Sticky); }
    bool unicode() const { return m_flags.contains(Flags::Unicode); }
    bool dotAll() const { return m_flags.contains(Flags::DotAll); }

    bool m_containsBackreferences : 1;
    bool m_containsBOL : 1;
    bool m_containsUnsignedLengthPattern : 1;
    bool m_hasCopiedParenSubexpressions : 1;
    bool m_saveInitialStartValue : 1;
    OptionSet<Flags> m_flags;
    unsigned m_numSubpatterns { 0 };
    unsigned m_initialStartValueFrameLocation { 0 };
    PatternDisjunction* m_body;
    Vector<std::unique_ptr<PatternDisjunction>, 4> m_disjunctions;
    Vector<std::unique_ptr<CharacterClass>> m_userCharacterClasses;
    Vector<String> m_captureGroupNames;
    HashMap<String, unsigned> m_namedGroupToParenIndex;

private:
    ErrorCode compile(const String& patternString);

    CharacterClass* anycharCached { nullptr };
    CharacterClass* newlineCached { nullptr };
    CharacterClass* digitsCached { nullptr };
    CharacterClass* spacesCached { nullptr };
    CharacterClass* wordcharCached { nullptr };
    CharacterClass* wordUnicodeIgnoreCaseCharCached { nullptr };
    CharacterClass* nondigitsCached { nullptr };
    CharacterClass* nonspacesCached { nullptr };
    CharacterClass* nonwordcharCached { nullptr };
    CharacterClass* nonwordUnicodeIgnoreCasecharCached { nullptr };
    HashMap<unsigned, CharacterClass*> unicodePropertiesCached;
};

    void indentForNestingLevel(PrintStream&, unsigned);
    void dumpUChar32(PrintStream&, UChar32);
    void dumpCharacterClass(PrintStream&, YarrPattern*, CharacterClass*);

    struct BackTrackInfoPatternCharacter {
        uintptr_t begin; // Only needed for unicode patterns
        uintptr_t matchAmount;

        static unsigned beginIndex() { return offsetof(BackTrackInfoPatternCharacter, begin) / sizeof(uintptr_t); }
        static unsigned matchAmountIndex() { return offsetof(BackTrackInfoPatternCharacter, matchAmount) / sizeof(uintptr_t); }
    };

    struct BackTrackInfoCharacterClass {
        uintptr_t begin; // Only needed for unicode patterns
        uintptr_t matchAmount;

        static unsigned beginIndex() { return offsetof(BackTrackInfoCharacterClass, begin) / sizeof(uintptr_t); }
        static unsigned matchAmountIndex() { return offsetof(BackTrackInfoCharacterClass, matchAmount) / sizeof(uintptr_t); }
    };

    struct BackTrackInfoBackReference {
        uintptr_t begin; // Not really needed for greedy quantifiers.
        uintptr_t matchAmount; // Not really needed for fixed quantifiers.

        static unsigned beginIndex() { return offsetof(BackTrackInfoBackReference, begin) / sizeof(uintptr_t); }
        static unsigned matchAmountIndex() { return offsetof(BackTrackInfoBackReference, matchAmount) / sizeof(uintptr_t); }
    };

    struct BackTrackInfoAlternative {
        union {
            uintptr_t offset;
        };
    };

    struct BackTrackInfoParentheticalAssertion {
        uintptr_t begin;

        static unsigned beginIndex() { return offsetof(BackTrackInfoParentheticalAssertion, begin) / sizeof(uintptr_t); }
    };

    struct BackTrackInfoParenthesesOnce {
        uintptr_t begin;
        uintptr_t returnAddress;

        static unsigned beginIndex() { return offsetof(BackTrackInfoParenthesesOnce, begin) / sizeof(uintptr_t); }
        static unsigned returnAddressIndex() { return offsetof(BackTrackInfoParenthesesOnce, returnAddress) / sizeof(uintptr_t); }
    };

    struct BackTrackInfoParenthesesTerminal {
        uintptr_t begin;

        static unsigned beginIndex() { return offsetof(BackTrackInfoParenthesesTerminal, begin) / sizeof(uintptr_t); }
    };

    struct BackTrackInfoParentheses {
        uintptr_t begin;
        uintptr_t returnAddress;
        uintptr_t matchAmount;
        uintptr_t parenContextHead;

        static unsigned beginIndex() { return offsetof(BackTrackInfoParentheses, begin) / sizeof(uintptr_t); }
        static unsigned returnAddressIndex() { return offsetof(BackTrackInfoParentheses, returnAddress) / sizeof(uintptr_t); }
        static unsigned matchAmountIndex() { return offsetof(BackTrackInfoParentheses, matchAmount) / sizeof(uintptr_t); }
        static unsigned parenContextHeadIndex() { return offsetof(BackTrackInfoParentheses, parenContextHead) / sizeof(uintptr_t); }
    };

} } // namespace JSC::Yarr
