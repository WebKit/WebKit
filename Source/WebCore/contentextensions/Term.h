/*
 * Copyright (C) 2014-2015 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef Term_h
#define Term_h

#if ENABLE(CONTENT_EXTENSIONS)

#include "NFA.h"
#include <unicode/utypes.h>
#include <wtf/ASCIICType.h>
#include <wtf/BitVector.h>
#include <wtf/HashMap.h>
#include <wtf/Vector.h>

namespace WebCore {

namespace ContentExtensions {

enum class AtomQuantifier : uint8_t {
    One,
    ZeroOrOne,
    ZeroOrMore,
    OneOrMore
};

class Term {
public:
    Term();
    Term(char character, bool isCaseSensitive);

    enum UniversalTransitionTag { UniversalTransition };
    explicit Term(UniversalTransitionTag);

    enum CharacterSetTermTag { CharacterSetTerm };
    explicit Term(CharacterSetTermTag, bool isInverted);

    enum GroupTermTag { GroupTerm };
    explicit Term(GroupTermTag);

    enum EndOfLineAssertionTermTag { EndOfLineAssertionTerm };
    explicit Term(EndOfLineAssertionTermTag);

    Term(const Term& other);
    Term(Term&& other);

    enum EmptyValueTag { EmptyValue };
    Term(EmptyValueTag);

    enum DeletedValueTag { DeletedValue };
    Term(DeletedValueTag);

    ~Term();

    bool isValid() const;

    // CharacterSet terms only.
    void addCharacter(UChar character, bool isCaseSensitive);

    // Group terms only.
    void extendGroupSubpattern(const Term&);

    void quantify(const AtomQuantifier&);

    unsigned generateGraph(NFA&, unsigned start, const ActionSet& finalActions) const;

    bool isEndOfLineAssertion() const;

    bool matchesAtLeastOneCharacter() const;

    // Matches any string, the empty string included.
    // This is very conservative. Patterns matching any string can return false here.
    bool isKnownToMatchAnyString() const;

    // Return true if the term can only match input of a single fixed length.
    bool hasFixedLength() const;

    Term& operator=(const Term& other);
    Term& operator=(Term&& other);

    bool operator==(const Term& other) const;

    unsigned hash() const;

    bool isEmptyValue() const;
    bool isDeletedValue() const;

private:
    // This is exact for character sets but conservative for groups.
    // The return value can be false for a group equivalent to a universal transition.
    bool isUniversalTransition() const;

    unsigned generateSubgraphForAtom(NFA&, unsigned source) const;

    void destroy();

    enum class TermType : uint8_t {
        Empty,
        Deleted,
        CharacterSet,
        Group
    };

    TermType m_termType { TermType::Empty };
    AtomQuantifier m_quantifier { AtomQuantifier::One };

    struct CharacterSet {
        bool inverted { false };
        BitVector characters { 128 };

        bool operator==(const CharacterSet& other) const
        {
            return other.inverted == inverted && other.characters == characters;
        }

        unsigned hash() const
        {
            return WTF::pairIntHash(inverted, characters.hash());
        }
    };

    struct Group {
        Vector<Term> terms;

        bool operator==(const Group& other) const
        {
            return other.terms == terms;
        }

        unsigned hash() const
        {
            unsigned hash = 6421749;
            for (const Term& term : terms) {
                unsigned termHash = term.hash();
                hash = (hash << 16) ^ ((termHash << 11) ^ hash);
                hash += hash >> 11;
            }
            return hash;
        }
    };

    union AtomData {
        AtomData()
            : invalidTerm(0)
        {
        }
        ~AtomData()
        {
        }

        char invalidTerm;
        CharacterSet characterSet;
        Group group;
    } m_atomData;
};

struct TermHash {
    static unsigned hash(const Term& term) { return term.hash(); }
    static bool equal(const Term& a, const Term& b) { return a == b; }
    static const bool safeToCompareToEmptyOrDeleted = true;
};

struct TermHashTraits : public WTF::CustomHashTraits<Term> { };

inline Term::Term()
{
}

inline Term::Term(char character, bool isCaseSensitive)
    : m_termType(TermType::CharacterSet)
{
    new (NotNull, &m_atomData.characterSet) CharacterSet();
    addCharacter(character, isCaseSensitive);
}

enum UniversalTransitionTag { UniversalTransition };
inline Term::Term(UniversalTransitionTag)
    : m_termType(TermType::CharacterSet)
{
    new (NotNull, &m_atomData.characterSet) CharacterSet();
    for (unsigned i = 0; i < 128; ++i)
        m_atomData.characterSet.characters.set(i);
}

inline Term::Term(CharacterSetTermTag, bool isInverted)
    : m_termType(TermType::CharacterSet)
{
    new (NotNull, &m_atomData.characterSet) CharacterSet();
    m_atomData.characterSet.inverted = isInverted;
}

inline Term::Term(GroupTermTag)
    : m_termType(TermType::Group)
{
    new (NotNull, &m_atomData.group) Group();
}

inline Term::Term(EndOfLineAssertionTermTag)
    : Term(CharacterSetTerm, false)
{
    m_atomData.characterSet.characters.set(0);
}

inline Term::Term(const Term& other)
    : m_termType(other.m_termType)
    , m_quantifier(other.m_quantifier)
{
    switch (m_termType) {
    case TermType::Empty:
    case TermType::Deleted:
        break;
    case TermType::CharacterSet:
        new (NotNull, &m_atomData.characterSet) CharacterSet(other.m_atomData.characterSet);
        break;
    case TermType::Group:
        new (NotNull, &m_atomData.group) Group(other.m_atomData.group);
        break;
    }
}

inline Term::Term(Term&& other)
    : m_termType(WTF::move(other.m_termType))
    , m_quantifier(WTF::move(other.m_quantifier))
{
    switch (m_termType) {
    case TermType::Empty:
    case TermType::Deleted:
        break;
    case TermType::CharacterSet:
        new (NotNull, &m_atomData.characterSet) CharacterSet(WTF::move(other.m_atomData.characterSet));
        break;
    case TermType::Group:
        new (NotNull, &m_atomData.group) Group(WTF::move(other.m_atomData.group));
        break;
    }
    other.destroy();
}

inline Term::Term(EmptyValueTag)
    : m_termType(TermType::Empty)
{
}

inline Term::Term(DeletedValueTag)
    : m_termType(TermType::Deleted)
{
}

inline Term::~Term()
{
    destroy();
}

inline bool Term::isValid() const
{
    return m_termType != TermType::Empty && m_termType != TermType::Deleted;
}

inline void Term::addCharacter(UChar character, bool isCaseSensitive)
{
    ASSERT(isASCII(character));

    ASSERT_WITH_SECURITY_IMPLICATION(m_termType == TermType::CharacterSet);
    if (m_termType != TermType::CharacterSet)
        return;

    if (isCaseSensitive || !isASCIIAlpha(character))
        m_atomData.characterSet.characters.set(character);
    else {
        m_atomData.characterSet.characters.set(toASCIIUpper(character));
        m_atomData.characterSet.characters.set(toASCIILower(character));
    }
}

inline void Term::extendGroupSubpattern(const Term& term)
{
    ASSERT_WITH_SECURITY_IMPLICATION(m_termType == TermType::Group);
    if (m_termType != TermType::Group)
        return;
    m_atomData.group.terms.append(term);
}

inline void Term::quantify(const AtomQuantifier& quantifier)
{
    ASSERT_WITH_MESSAGE(m_quantifier == AtomQuantifier::One, "Transition to quantified term should only happen once.");
    m_quantifier = quantifier;
}

inline unsigned Term::Term::generateGraph(NFA& nfa, unsigned start, const ActionSet& finalActions) const
{
    ASSERT(isValid());

    unsigned newEnd;

    switch (m_quantifier) {
    case AtomQuantifier::One: {
        newEnd = generateSubgraphForAtom(nfa, start);
        break;
    }
    case AtomQuantifier::ZeroOrOne: {
        newEnd = generateSubgraphForAtom(nfa, start);
        nfa.addEpsilonTransition(start, newEnd);
        break;
    }
    case AtomQuantifier::ZeroOrMore: {
        unsigned repeatStart = nfa.createNode();
        nfa.addEpsilonTransition(start, repeatStart);

        unsigned repeatEnd = generateSubgraphForAtom(nfa, repeatStart);
        nfa.addEpsilonTransition(repeatEnd, repeatStart);

        unsigned kleenEnd = nfa.createNode();
        nfa.addEpsilonTransition(repeatEnd, kleenEnd);
        nfa.addEpsilonTransition(start, kleenEnd);
        newEnd = kleenEnd;
        break;
    }
    case AtomQuantifier::OneOrMore: {
        unsigned repeatStart = nfa.createNode();
        nfa.addEpsilonTransition(start, repeatStart);

        unsigned repeatEnd = generateSubgraphForAtom(nfa, repeatStart);
        nfa.addEpsilonTransition(repeatEnd, repeatStart);

        unsigned afterRepeat = nfa.createNode();
        nfa.addEpsilonTransition(repeatEnd, afterRepeat);
        newEnd = afterRepeat;
        break;
    }
    }
    nfa.setActions(newEnd, finalActions);
    return newEnd;
}

inline bool Term::isEndOfLineAssertion() const
{
    return m_termType == TermType::CharacterSet && m_atomData.characterSet.characters.bitCount() == 1 && m_atomData.characterSet.characters.get(0);
}

inline bool Term::matchesAtLeastOneCharacter() const
{
    ASSERT(isValid());

    if (m_quantifier == AtomQuantifier::ZeroOrOne || m_quantifier == AtomQuantifier::ZeroOrMore)
        return false;
    if (isEndOfLineAssertion())
        return false;

    if (m_termType == TermType::Group) {
        for (const Term& term : m_atomData.group.terms) {
            if (term.matchesAtLeastOneCharacter())
                return true;
        }
        return false;
    }
    return true;
}

inline bool Term::isKnownToMatchAnyString() const
{
    ASSERT(isValid());

    switch (m_termType) {
    case TermType::Empty:
    case TermType::Deleted:
        ASSERT_NOT_REACHED();
        break;
    case TermType::CharacterSet:
        // ".*" is the only simple term matching any string.
        return isUniversalTransition() && m_quantifier == AtomQuantifier::ZeroOrMore;
        break;
    case TermType::Group: {
        // There are infinitely many ways to match anything with groups, we just handle simple cases
        if (m_atomData.group.terms.size() != 1)
            return false;

        const Term& firstTermInGroup = m_atomData.group.terms.first();
        // -(.*) with any quantifier.
        if (firstTermInGroup.isKnownToMatchAnyString())
            return true;

        if (firstTermInGroup.isUniversalTransition()) {
            // -(.)*, (.+)*, (.?)* etc.
            if (m_quantifier == AtomQuantifier::ZeroOrMore)
                return true;

            // -(.+)?.
            if (m_quantifier == AtomQuantifier::ZeroOrOne && firstTermInGroup.m_quantifier == AtomQuantifier::OneOrMore)
                return true;

            // -(.?)+.
            if (m_quantifier == AtomQuantifier::OneOrMore && firstTermInGroup.m_quantifier == AtomQuantifier::ZeroOrOne)
                return true;
        }
        break;
    }
    }
    return false;
}

inline bool Term::hasFixedLength() const
{
    ASSERT(isValid());

    switch (m_termType) {
    case TermType::Empty:
    case TermType::Deleted:
        ASSERT_NOT_REACHED();
        break;
    case TermType::CharacterSet:
        return m_quantifier == AtomQuantifier::One;
    case TermType::Group: {
        if (m_quantifier != AtomQuantifier::One)
            return false;
        for (const Term& term : m_atomData.group.terms) {
            if (!term.hasFixedLength())
                return false;
        }
        return true;
    }
    }
    return false;
}

inline Term& Term::operator=(const Term& other)
{
    destroy();
    new (NotNull, this) Term(other);
    return *this;
}

inline Term& Term::operator=(Term&& other)
{
    destroy();
    new (NotNull, this) Term(WTF::move(other));
    return *this;
}

inline bool Term::operator==(const Term& other) const
{
    if (other.m_termType != m_termType || other.m_quantifier != m_quantifier)
        return false;

    switch (m_termType) {
    case TermType::Empty:
    case TermType::Deleted:
        return true;
    case TermType::CharacterSet:
        return m_atomData.characterSet == other.m_atomData.characterSet;
    case TermType::Group:
        return m_atomData.group == other.m_atomData.group;
    }
    ASSERT_NOT_REACHED();
    return false;
}

inline unsigned Term::hash() const
{
    unsigned primary = static_cast<unsigned>(m_termType) << 16 | static_cast<unsigned>(m_quantifier);
    unsigned secondary = 0;
    switch (m_termType) {
    case TermType::Empty:
        secondary = 52184393;
        break;
    case TermType::Deleted:
        secondary = 40342988;
        break;
    case TermType::CharacterSet:
        secondary = m_atomData.characterSet.hash();
        break;
    case TermType::Group:
        secondary = m_atomData.group.hash();
        break;
    }
    return WTF::pairIntHash(primary, secondary);
}

inline bool Term::isEmptyValue() const
{
    return m_termType == TermType::Empty;
}

inline bool Term::isDeletedValue() const
{
    return m_termType == TermType::Deleted;
}

inline bool Term::isUniversalTransition() const
{
    ASSERT(isValid());

    switch (m_termType) {
    case TermType::Empty:
    case TermType::Deleted:
        ASSERT_NOT_REACHED();
        break;
    case TermType::CharacterSet:
        return (m_atomData.characterSet.inverted && !m_atomData.characterSet.characters.bitCount())
            || (!m_atomData.characterSet.inverted && m_atomData.characterSet.characters.bitCount() == 128);
    case TermType::Group:
        return m_atomData.group.terms.size() == 1 && m_atomData.group.terms.first().isUniversalTransition();
    }
    return false;
}

inline unsigned Term::generateSubgraphForAtom(NFA& nfa, unsigned source) const
{
    switch (m_termType) {
    case TermType::Empty:
    case TermType::Deleted:
        ASSERT_NOT_REACHED();
        return -1;
    case TermType::CharacterSet: {
        unsigned target = nfa.createNode();
        if (isUniversalTransition())
            nfa.addTransitionsOnAnyCharacter(source, target);
        else {
            if (!m_atomData.characterSet.inverted) {
                for (const auto& characterIterator : m_atomData.characterSet.characters.setBits())
                    nfa.addTransition(source, target, static_cast<char>(characterIterator));
            } else {
                for (unsigned i = 1; i < m_atomData.characterSet.characters.size(); ++i) {
                    if (m_atomData.characterSet.characters.get(i))
                        continue;
                    nfa.addTransition(source, target, static_cast<char>(i));
                }
            }
        }
        return target;
    }
    case TermType::Group: {
        unsigned lastTarget = source;
        for (const Term& term : m_atomData.group.terms)
            lastTarget = term.generateGraph(nfa, lastTarget, ActionSet());
        return lastTarget;
    }
    }
}

inline void Term::destroy()
{
    switch (m_termType) {
    case TermType::Empty:
    case TermType::Deleted:
        break;
    case TermType::CharacterSet:
        m_atomData.characterSet.~CharacterSet();
        break;
    case TermType::Group:
        m_atomData.group.~Group();
        break;
    }
    m_termType = TermType::Deleted;
}

} // namespace ContentExtensions
} // namespace WebCore

#endif // ENABLE(CONTENT_EXTENSIONS)

#endif // Term_h
