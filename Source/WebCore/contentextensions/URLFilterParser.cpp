/*
 * Copyright (C) 2014, 2015 Apple Inc. All rights reserved.
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

#include "config.h"
#include "URLFilterParser.h"

#if ENABLE(CONTENT_EXTENSIONS)

#include "NFA.h"
#include <JavaScriptCore/YarrParser.h>
#include <wtf/BitVector.h>
#include <wtf/Deque.h>
#include <wtf/text/CString.h>

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
    Term()
    {
    }

    Term(char character, bool isCaseSensitive)
        : m_termType(TermType::CharacterSet)
    {
        new (NotNull, &m_atomData.characterSet) CharacterSet();
        addCharacter(character, isCaseSensitive);
    }

    enum UniversalTransitionTag { UniversalTransition };
    explicit Term(UniversalTransitionTag)
        : m_termType(TermType::CharacterSet)
    {
        new (NotNull, &m_atomData.characterSet) CharacterSet();
        for (unsigned i = 0; i < 128; ++i)
            m_atomData.characterSet.characters.set(i);
    }

    enum CharacterSetTermTag { CharacterSetTerm };
    explicit Term(CharacterSetTermTag, bool isInverted)
        : m_termType(TermType::CharacterSet)
    {
        new (NotNull, &m_atomData.characterSet) CharacterSet();
        m_atomData.characterSet.inverted = isInverted;
    }

    enum GroupTermTag { GroupTerm };
    explicit Term(GroupTermTag)
        : m_termType(TermType::Group)
    {
        new (NotNull, &m_atomData.group) Group();
    }

    enum EndOfLineAssertionTermTag { EndOfLineAssertionTerm };
    explicit Term(EndOfLineAssertionTermTag)
        : Term(CharacterSetTerm, false)
    {
        m_atomData.characterSet.characters.set(0);
    }

    Term(const Term& other)
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

    Term(Term&& other)
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

    enum EmptyValueTag { EmptyValue };
    Term(EmptyValueTag)
        : m_termType(TermType::Empty)
    {
    }

    enum DeletedValueTag { DeletedValue };
    Term(DeletedValueTag)
        : m_termType(TermType::Deleted)
    {
    }

    ~Term()
    {
        destroy();
    }

    bool isValid() const
    {
        return m_termType != TermType::Empty && m_termType != TermType::Deleted;
    }

    void addCharacter(UChar character, bool isCaseSensitive)
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

    void extendGroupSubpattern(const Term& term)
    {
        ASSERT_WITH_SECURITY_IMPLICATION(m_termType == TermType::Group);
        if (m_termType != TermType::Group)
            return;
        m_atomData.group.terms.append(term);
    }

    void quantify(const AtomQuantifier& quantifier)
    {
        ASSERT_WITH_MESSAGE(m_quantifier == AtomQuantifier::One, "Transition to quantified term should only happen once.");
        m_quantifier = quantifier;
    }

    unsigned generateGraph(NFA& nfa, uint64_t patternId, unsigned start) const
    {
        ASSERT(isValid());

        switch (m_quantifier) {
        case AtomQuantifier::One: {
            unsigned newEnd = generateSubgraphForAtom(nfa, patternId, start);
            return newEnd;
        }
        case AtomQuantifier::ZeroOrOne: {
            unsigned newEnd = generateSubgraphForAtom(nfa, patternId, start);
            nfa.addEpsilonTransition(start, newEnd);
            return newEnd;
        }
        case AtomQuantifier::ZeroOrMore: {
            unsigned repeatStart = nfa.createNode();
            nfa.addRuleId(repeatStart, patternId);
            nfa.addEpsilonTransition(start, repeatStart);

            unsigned repeatEnd = generateSubgraphForAtom(nfa, patternId, repeatStart);
            nfa.addEpsilonTransition(repeatEnd, repeatStart);

            unsigned kleenEnd = nfa.createNode();
            nfa.addRuleId(kleenEnd, patternId);
            nfa.addEpsilonTransition(repeatEnd, kleenEnd);
            nfa.addEpsilonTransition(start, kleenEnd);
            return kleenEnd;
        }
        case AtomQuantifier::OneOrMore: {
            unsigned repeatStart = nfa.createNode();
            nfa.addRuleId(repeatStart, patternId);
            nfa.addEpsilonTransition(start, repeatStart);

            unsigned repeatEnd = generateSubgraphForAtom(nfa, patternId, repeatStart);
            nfa.addEpsilonTransition(repeatEnd, repeatStart);

            unsigned afterRepeat = nfa.createNode();
            nfa.addRuleId(afterRepeat, patternId);
            nfa.addEpsilonTransition(repeatEnd, afterRepeat);
            return afterRepeat;
        }
        }
    }

    bool isEndOfLineAssertion() const
    {
        return m_termType == TermType::CharacterSet && m_atomData.characterSet.characters.bitCount() == 1 && m_atomData.characterSet.characters.get(0);
    }

    bool matchesAtLeastOneCharacter() const
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

    // Matches any string, the empty string included.
    // This is very conservative. Patterns matching any string can return false here.
    bool isKnownToMatchAnyString() const
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

    Term& operator=(const Term& other)
    {
        destroy();
        new (NotNull, this) Term(other);
        return *this;
    }

    Term& operator=(Term&& other)
    {
        destroy();
        new (NotNull, this) Term(WTF::move(other));
        return *this;
    }

    bool operator==(const Term& other) const
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

    unsigned hash() const
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

    bool isEmptyValue() const
    {
        return m_termType == TermType::Empty;
    }

    bool isDeletedValue() const
    {
        return m_termType == TermType::Deleted;
    }

private:
    // This is exact for character sets but conservative for groups.
    // The return value can be false for a group equivalent to a universal transition.
    bool isUniversalTransition() const
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

    unsigned generateSubgraphForAtom(NFA& nfa, uint64_t patternId, unsigned source) const
    {
        switch (m_termType) {
        case TermType::Empty:
        case TermType::Deleted:
            ASSERT_NOT_REACHED();
            return -1;
        case TermType::CharacterSet: {
            unsigned target = nfa.createNode();
            nfa.addRuleId(target, patternId);
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
                lastTarget = term.generateGraph(nfa, patternId, lastTarget);
            return lastTarget;
        }
        }
    }

    void destroy()
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

struct PrefixTreeEntry {
    unsigned nfaNode;
    HashMap<Term, std::unique_ptr<PrefixTreeEntry>, TermHash, TermHashTraits> nextPattern;
};

class GraphBuilder {
public:
    GraphBuilder(NFA& nfa, PrefixTreeEntry& prefixTreeRoot, bool patternIsCaseSensitive, uint64_t patternId)
        : m_nfa(nfa)
        , m_patternIsCaseSensitive(patternIsCaseSensitive)
        , m_patternId(patternId)
        , m_subtreeStart(nfa.root())
        , m_subtreeEnd(nfa.root())
        , m_lastPrefixTreeEntry(&prefixTreeRoot)
        , m_parseStatus(URLFilterParser::Ok)
    {
    }

    void finalize()
    {
        if (hasError())
            return;

        sinkFloatingTermIfNecessary();

        simplifySunkTerms();

        // Check to see if there are any terms without ? or *.
        bool matchesEverything = true;
        for (const auto& term : m_sunkTerms) {
            if (term.matchesAtLeastOneCharacter()) {
                matchesEverything = false;
                break;
            }
        }
        if (matchesEverything) {
            fail(URLFilterParser::MatchesEverything);
            return;
        }

        for (const auto& term : m_sunkTerms) {
            ASSERT(m_lastPrefixTreeEntry);
            auto nextEntry = m_lastPrefixTreeEntry->nextPattern.find(term);
            if (nextEntry != m_lastPrefixTreeEntry->nextPattern.end()) {
                m_lastPrefixTreeEntry = nextEntry->value.get();
                m_nfa.addRuleId(m_lastPrefixTreeEntry->nfaNode, m_patternId);
            } else {
                std::unique_ptr<PrefixTreeEntry> nextPrefixTreeEntry = std::make_unique<PrefixTreeEntry>();
                
                unsigned newEnd = term.generateGraph(m_nfa, m_patternId, m_lastPrefixTreeEntry->nfaNode);
                nextPrefixTreeEntry->nfaNode = newEnd;
                
                auto addResult = m_lastPrefixTreeEntry->nextPattern.set(term, WTF::move(nextPrefixTreeEntry));
                ASSERT(addResult.isNewEntry);

                m_lastPrefixTreeEntry = addResult.iterator->value.get();
            }
            m_subtreeEnd = m_lastPrefixTreeEntry->nfaNode;
        }
        
        ASSERT_WITH_MESSAGE(m_openGroups.isEmpty(), "An unclosed group should be a parsing error in YARR.");
        ASSERT_WITH_MESSAGE(m_subtreeStart != m_subtreeEnd, "This regex cannot match anything");
        
        m_nfa.setFinal(m_subtreeEnd, m_patternId);
    }

    URLFilterParser::ParseStatus parseStatus() const
    {
        return m_parseStatus;
    }

    void atomPatternCharacter(UChar character)
    {
        if (hasError())
            return;

        if (!isASCII(character)) {
            fail(URLFilterParser::NonASCII);
            return;
        }

        sinkFloatingTermIfNecessary();
        ASSERT(!m_floatingTerm.isValid());

        char asciiChararacter = static_cast<char>(character);
        m_floatingTerm = Term(asciiChararacter, m_patternIsCaseSensitive);
    }

    void atomBuiltInCharacterClass(JSC::Yarr::BuiltInCharacterClassID builtInCharacterClassID, bool inverted)
    {
        if (hasError())
            return;

        sinkFloatingTermIfNecessary();
        ASSERT(!m_floatingTerm.isValid());

        if (builtInCharacterClassID == JSC::Yarr::NewlineClassID && inverted)
            m_floatingTerm = Term(Term::UniversalTransition);
        else
            fail(URLFilterParser::UnsupportedCharacterClass);
    }

    void quantifyAtom(unsigned minimum, unsigned maximum, bool)
    {
        if (hasError())
            return;

        ASSERT(m_floatingTerm.isValid());

        if (!minimum && maximum == 1)
            m_floatingTerm.quantify(AtomQuantifier::ZeroOrOne);
        else if (!minimum && maximum == JSC::Yarr::quantifyInfinite)
            m_floatingTerm.quantify(AtomQuantifier::ZeroOrMore);
        else if (minimum == 1 && maximum == JSC::Yarr::quantifyInfinite)
            m_floatingTerm.quantify(AtomQuantifier::OneOrMore);
        else
            fail(URLFilterParser::InvalidQuantifier);
    }

    void atomBackReference(unsigned)
    {
        fail(URLFilterParser::BackReference);
    }

    void assertionBOL()
    {
        if (hasError())
            return;

        if (m_subtreeStart != m_subtreeEnd || m_floatingTerm.isValid() || !m_openGroups.isEmpty()) {
            fail(URLFilterParser::MisplacedStartOfLine);
            return;
        }

        m_hasBeginningOfLineAssertion = true;
    }

    void assertionEOL()
    {
        if (hasError())
            return;

        sinkFloatingTermIfNecessary();
        ASSERT(!m_floatingTerm.isValid());

        m_floatingTerm = Term(Term::EndOfLineAssertionTerm);
    }

    void assertionWordBoundary(bool)
    {
        fail(URLFilterParser::WordBoundary);
    }

    void atomCharacterClassBegin(bool inverted = false)
    {
        if (hasError())
            return;

        sinkFloatingTermIfNecessary();
        ASSERT(!m_floatingTerm.isValid());

        m_floatingTerm = Term(Term::CharacterSetTerm, inverted);
    }

    void atomCharacterClassAtom(UChar character)
    {
        if (hasError())
            return;

        ASSERT(isASCII(character));

        m_floatingTerm.addCharacter(character, m_patternIsCaseSensitive);
    }

    void atomCharacterClassRange(UChar a, UChar b)
    {
        if (hasError())
            return;

        ASSERT(a);
        ASSERT(b);
        ASSERT(isASCII(a));
        ASSERT(isASCII(b));

        for (unsigned i = a; i <= b; ++i)
            m_floatingTerm.addCharacter(static_cast<UChar>(i), m_patternIsCaseSensitive);
    }

    void atomCharacterClassEnd()
    {
        // Nothing to do here. The character set atom may have a quantifier, we sink the atom lazily.
    }

    void atomCharacterClassBuiltIn(JSC::Yarr::BuiltInCharacterClassID, bool)
    {
        fail(URLFilterParser::AtomCharacter);
    }

    void atomParenthesesSubpatternBegin(bool = true)
    {
        if (hasError())
            return;

        sinkFloatingTermIfNecessary();

        m_openGroups.append(Term(Term::GroupTerm));
    }

    void atomParentheticalAssertionBegin(bool = false)
    {
        fail(URLFilterParser::Group);
    }

    void atomParenthesesEnd()
    {
        if (hasError())
            return;

        sinkFloatingTermIfNecessary();
        ASSERT(!m_floatingTerm.isValid());

        m_floatingTerm = m_openGroups.takeLast();
    }

    void disjunction()
    {
        fail(URLFilterParser::Disjunction);
    }

private:
    bool hasError() const
    {
        return m_parseStatus != URLFilterParser::Ok;
    }

    void fail(URLFilterParser::ParseStatus reason)
    {
        if (hasError())
            return;

        m_parseStatus = reason;
    }

    void sinkFloatingTermIfNecessary()
    {
        if (!m_floatingTerm.isValid())
            return;

        if (m_hasProcessedEndOfLineAssertion) {
            fail(URLFilterParser::MisplacedEndOfLine);
            m_floatingTerm = Term();
            return;
        }

        if (m_floatingTerm.isEndOfLineAssertion())
            m_hasProcessedEndOfLineAssertion = true;

        if (!m_openGroups.isEmpty()) {
            m_openGroups.last().extendGroupSubpattern(m_floatingTerm);
            m_floatingTerm = Term();
            return;
        }

        m_sunkTerms.append(m_floatingTerm);
        m_floatingTerm = Term();
    }

    void simplifySunkTerms()
    {
        ASSERT(!m_floatingTerm.isValid());

        if (m_sunkTerms.isEmpty())
            return;

        Term canonicalDotStar(Term::UniversalTransition);
        canonicalDotStar.quantify(AtomQuantifier::ZeroOrMore);

        // Replace every ".*"-like terms by our canonical version. Remove any duplicate ".*".
        {
            unsigned termIndex = 0;
            bool isAfterDotStar = false;
            while (termIndex < m_sunkTerms.size()) {
                if (isAfterDotStar && m_sunkTerms[termIndex].isKnownToMatchAnyString()) {
                    m_sunkTerms.remove(termIndex);
                    continue;
                }
                isAfterDotStar = false;

                if (m_sunkTerms[termIndex].isKnownToMatchAnyString()) {
                    m_sunkTerms[termIndex] = canonicalDotStar;
                    isAfterDotStar = true;
                }
                ++termIndex;
            }
        }

        // Add our ".*" in front if needed.
        if (!m_hasBeginningOfLineAssertion && !m_sunkTerms.first().isKnownToMatchAnyString())
            m_sunkTerms.insert(0, canonicalDotStar);

        // Remove trailing ".*$".
        if (m_sunkTerms.size() > 2 && m_sunkTerms.last().isEndOfLineAssertion() && m_sunkTerms[m_sunkTerms.size() - 2].isKnownToMatchAnyString())
            m_sunkTerms.shrink(m_sunkTerms.size() - 2);

        // Remove irrelevant terms that can match empty. For example in "foob?", matching "b" is irrelevant.
        if (m_sunkTerms.last().isEndOfLineAssertion())
            return;
        while (!m_sunkTerms.isEmpty() && !m_sunkTerms.last().matchesAtLeastOneCharacter())
            m_sunkTerms.removeLast();
    }

    NFA& m_nfa;
    bool m_patternIsCaseSensitive;
    const uint64_t m_patternId;

    unsigned m_subtreeStart { 0 };
    unsigned m_subtreeEnd { 0 };

    PrefixTreeEntry* m_lastPrefixTreeEntry;
    Deque<Term> m_openGroups;
    Vector<Term> m_sunkTerms;
    Term m_floatingTerm;
    bool m_hasBeginningOfLineAssertion { false };
    bool m_hasProcessedEndOfLineAssertion { false };

    Term m_newPrefixStaringPoint;

    URLFilterParser::ParseStatus m_parseStatus;
};

URLFilterParser::URLFilterParser(NFA& nfa)
    : m_nfa(nfa)
    , m_prefixTreeRoot(std::make_unique<PrefixTreeEntry>())
{
    m_prefixTreeRoot->nfaNode = nfa.root();
}

URLFilterParser::~URLFilterParser()
{
}

URLFilterParser::ParseStatus URLFilterParser::addPattern(const String& pattern, bool patternIsCaseSensitive, uint64_t patternId)
{
    if (!pattern.containsOnlyASCII())
        return NonASCII;
    if (pattern.isEmpty())
        return EmptyPattern;

    unsigned oldSize = m_nfa.graphSize();

    ParseStatus status = Ok;
    GraphBuilder graphBuilder(m_nfa, *m_prefixTreeRoot, patternIsCaseSensitive, patternId);
    String error = String(JSC::Yarr::parse(graphBuilder, pattern, 0));
    if (error.isNull())
        graphBuilder.finalize();
    else
        status = YarrError;
    
    if (status == Ok)
        status = graphBuilder.parseStatus();

    if (status != Ok)
        m_nfa.restoreToGraphSize(oldSize);

    return status;
}

String URLFilterParser::statusString(ParseStatus status)
{
    switch (status) {
    case Ok:
        return "Ok";
    case MatchesEverything:
        return "Matches everything.";
    case NonASCII:
        return "Only ASCII characters are supported in pattern.";
    case UnsupportedCharacterClass:
        return "Character class is not supported.";
    case BackReference:
        return "Patterns cannot contain backreferences.";
    case MisplacedStartOfLine:
        return "Start of line assertion can only appear as the first term in a filter.";
    case WordBoundary:
        return "Word boundaries assertions are not supported yet.";
    case AtomCharacter:
        return "Builtins character class atoms are not supported yet.";
    case Group:
        return "Groups are not supported yet.";
    case Disjunction:
        return "Disjunctions are not supported yet.";
    case MisplacedEndOfLine:
        return "The end of line assertion must be the last term in an expression.";
    case EmptyPattern:
        return "Empty pattern.";
    case YarrError:
        return "Internal error in YARR.";
    case InvalidQuantifier:
        return "Arbitrary atom repetitions are not supported.";
    }
}
    
} // namespace ContentExtensions
} // namespace WebCore

#endif // ENABLE(CONTENT_EXTENSIONS)
