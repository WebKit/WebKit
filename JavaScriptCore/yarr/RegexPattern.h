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

#ifndef RegexPattern_h
#define RegexPattern_h


#if ENABLE(YARR)

#include <wtf/Vector.h>
#include <wtf/unicode/Unicode.h>


namespace JSC { namespace Yarr {

#define RegexStackSpaceForBackTrackInfoPatternCharacter 1 // Only for !fixed quantifiers.
#define RegexStackSpaceForBackTrackInfoCharacterClass 1 // Only for !fixed quantifiers.
#define RegexStackSpaceForBackTrackInfoBackReference 2
#define RegexStackSpaceForBackTrackInfoAlternative 1 // One per alternative.
#define RegexStackSpaceForBackTrackInfoParentheticalAssertion 1
#define RegexStackSpaceForBackTrackInfoParenthesesOnce 1 // Only for !fixed quantifiers.
#define RegexStackSpaceForBackTrackInfoParentheses 4

struct PatternDisjunction;

struct CharacterRange {
    UChar begin;
    UChar end;

    CharacterRange(UChar begin, UChar end)
        : begin(begin)
        , end(end)
    {
    }
};

struct CharacterClassTable : RefCounted<CharacterClassTable> {
    const char* m_table;
    bool m_inverted;
    static PassRefPtr<CharacterClassTable> create(const char* table, bool inverted)
    {
        return adoptRef(new CharacterClassTable(table, inverted));
    }

private:
    CharacterClassTable(const char* table, bool inverted)
        : m_table(table)
        , m_inverted(inverted)
    {
    }
};

struct CharacterClass : FastAllocBase {
    // All CharacterClass instances have to have the full set of matches and ranges,
    // they may have an optional table for faster lookups (which must match the
    // specified matches and ranges)
    CharacterClass(PassRefPtr<CharacterClassTable> table)
        : m_table(table)
    {
    }
    Vector<UChar> m_matches;
    Vector<CharacterRange> m_ranges;
    Vector<UChar> m_matchesUnicode;
    Vector<CharacterRange> m_rangesUnicode;
    RefPtr<CharacterClassTable> m_table;
};

enum QuantifierType {
    QuantifierFixedCount,
    QuantifierGreedy,
    QuantifierNonGreedy,
};

struct PatternTerm {
    enum Type {
        TypeAssertionBOL,
        TypeAssertionEOL,
        TypeAssertionWordBoundary,
        TypePatternCharacter,
        TypeCharacterClass,
        TypeBackReference,
        TypeForwardReference,
        TypeParenthesesSubpattern,
        TypeParentheticalAssertion,
    } type;
    bool invertOrCapture;
    union {
        UChar patternCharacter;
        CharacterClass* characterClass;
        unsigned subpatternId;
        struct {
            PatternDisjunction* disjunction;
            unsigned subpatternId;
            unsigned lastSubpatternId;
            bool isCopy;
        } parentheses;
    };
    QuantifierType quantityType;
    unsigned quantityCount;
    int inputPosition;
    unsigned frameLocation;

    PatternTerm(UChar ch)
        : type(PatternTerm::TypePatternCharacter)
    {
        patternCharacter = ch;
        quantityType = QuantifierFixedCount;
        quantityCount = 1;
    }

    PatternTerm(CharacterClass* charClass, bool invert)
        : type(PatternTerm::TypeCharacterClass)
        , invertOrCapture(invert)
    {
        characterClass = charClass;
        quantityType = QuantifierFixedCount;
        quantityCount = 1;
    }

    PatternTerm(Type type, unsigned subpatternId, PatternDisjunction* disjunction, bool invertOrCapture)
        : type(type)
        , invertOrCapture(invertOrCapture)
    {
        parentheses.disjunction = disjunction;
        parentheses.subpatternId = subpatternId;
        parentheses.isCopy = false;
        quantityType = QuantifierFixedCount;
        quantityCount = 1;
    }
    
    PatternTerm(Type type, bool invert = false)
        : type(type)
        , invertOrCapture(invert)
    {
        quantityType = QuantifierFixedCount;
        quantityCount = 1;
    }

    PatternTerm(unsigned spatternId)
        : type(TypeBackReference)
        , invertOrCapture(false)
    {
        subpatternId = spatternId;
        quantityType = QuantifierFixedCount;
        quantityCount = 1;
    }

    static PatternTerm ForwardReference()
    {
        return PatternTerm(TypeForwardReference);
    }

    static PatternTerm BOL()
    {
        return PatternTerm(TypeAssertionBOL);
    }

    static PatternTerm EOL()
    {
        return PatternTerm(TypeAssertionEOL);
    }

    static PatternTerm WordBoundary(bool invert)
    {
        return PatternTerm(TypeAssertionWordBoundary, invert);
    }
    
    bool invert()
    {
        return invertOrCapture;
    }

    bool capture()
    {
        return invertOrCapture;
    }
    
    void quantify(unsigned count, QuantifierType type)
    {
        quantityCount = count;
        quantityType = type;
    }
};

struct PatternAlternative : FastAllocBase {
    PatternAlternative(PatternDisjunction* disjunction)
        : m_parent(disjunction)
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

    Vector<PatternTerm> m_terms;
    PatternDisjunction* m_parent;
    unsigned m_minimumSize;
    bool m_hasFixedSize;
};

struct PatternDisjunction : FastAllocBase {
    PatternDisjunction(PatternAlternative* parent = 0)
        : m_parent(parent)
    {
    }
    
    ~PatternDisjunction()
    {
        deleteAllValues(m_alternatives);
    }

    PatternAlternative* addNewAlternative()
    {
        PatternAlternative* alternative = new PatternAlternative(this);
        m_alternatives.append(alternative);
        return alternative;
    }

    Vector<PatternAlternative*> m_alternatives;
    PatternAlternative* m_parent;
    unsigned m_minimumSize;
    unsigned m_callFrameSize;
    bool m_hasFixedSize;
};

// You probably don't want to be calling these functions directly
// (please to be calling newlineCharacterClass() et al on your
// friendly neighborhood RegexPattern instance to get nicely
// cached copies).
CharacterClass* newlineCreate();
CharacterClass* digitsCreate();
CharacterClass* spacesCreate();
CharacterClass* wordcharCreate();
CharacterClass* nondigitsCreate();
CharacterClass* nonspacesCreate();
CharacterClass* nonwordcharCreate();

struct RegexPattern {
    RegexPattern(bool ignoreCase, bool multiline)
        : m_ignoreCase(ignoreCase)
        , m_multiline(multiline)
        , m_numSubpatterns(0)
        , m_maxBackReference(0)
        , m_shouldFallBack(false)
        , newlineCached(0)
        , digitsCached(0)
        , spacesCached(0)
        , wordcharCached(0)
        , nondigitsCached(0)
        , nonspacesCached(0)
        , nonwordcharCached(0)
    {
    }

    ~RegexPattern()
    {
        deleteAllValues(m_disjunctions);
        deleteAllValues(m_userCharacterClasses);
    }

    void reset()
    {
        m_numSubpatterns = 0;
        m_maxBackReference = 0;

        m_shouldFallBack = false;

        newlineCached = 0;
        digitsCached = 0;
        spacesCached = 0;
        wordcharCached = 0;
        nondigitsCached = 0;
        nonspacesCached = 0;
        nonwordcharCached = 0;

        deleteAllValues(m_disjunctions);
        m_disjunctions.clear();
        deleteAllValues(m_userCharacterClasses);
        m_userCharacterClasses.clear();
    }

    bool containsIllegalBackReference()
    {
        return m_maxBackReference > m_numSubpatterns;
    }

    CharacterClass* newlineCharacterClass()
    {
        if (!newlineCached)
            m_userCharacterClasses.append(newlineCached = newlineCreate());
        return newlineCached;
    }
    CharacterClass* digitsCharacterClass()
    {
        if (!digitsCached)
            m_userCharacterClasses.append(digitsCached = digitsCreate());
        return digitsCached;
    }
    CharacterClass* spacesCharacterClass()
    {
        if (!spacesCached)
            m_userCharacterClasses.append(spacesCached = spacesCreate());
        return spacesCached;
    }
    CharacterClass* wordcharCharacterClass()
    {
        if (!wordcharCached)
            m_userCharacterClasses.append(wordcharCached = wordcharCreate());
        return wordcharCached;
    }
    CharacterClass* nondigitsCharacterClass()
    {
        if (!nondigitsCached)
            m_userCharacterClasses.append(nondigitsCached = nondigitsCreate());
        return nondigitsCached;
    }
    CharacterClass* nonspacesCharacterClass()
    {
        if (!nonspacesCached)
            m_userCharacterClasses.append(nonspacesCached = nonspacesCreate());
        return nonspacesCached;
    }
    CharacterClass* nonwordcharCharacterClass()
    {
        if (!nonwordcharCached)
            m_userCharacterClasses.append(nonwordcharCached = nonwordcharCreate());
        return nonwordcharCached;
    }

    bool m_ignoreCase;
    bool m_multiline;
    unsigned m_numSubpatterns;
    unsigned m_maxBackReference;
    bool m_shouldFallBack;
    PatternDisjunction* m_body;
    Vector<PatternDisjunction*, 4> m_disjunctions;
    Vector<CharacterClass*> m_userCharacterClasses;

private:
    CharacterClass* newlineCached;
    CharacterClass* digitsCached;
    CharacterClass* spacesCached;
    CharacterClass* wordcharCached;
    CharacterClass* nondigitsCached;
    CharacterClass* nonspacesCached;
    CharacterClass* nonwordcharCached;
};

} } // namespace JSC::Yarr

#endif

#endif // RegexPattern_h
