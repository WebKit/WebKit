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

namespace WebCore {

namespace ContentExtensions {

const uint16_t hasNonCharacterMask = 0x0080;
const uint16_t characterMask = 0x0007F;
const uint16_t newlineClassIDBuiltinMask = 0x100;
const uint16_t caseInsensitiveMask = 0x200;

static TrivialAtom trivialAtomFromASCIICharacter(char character, bool caseSensitive)
{
    ASSERT(isASCII(character));

    if (caseSensitive || !isASCIIAlpha(character))
        return static_cast<uint16_t>(character);

    return static_cast<uint16_t>(toASCIILower(character)) | caseInsensitiveMask;
}

enum class AtomQuantifier : uint16_t {
    One = 0,
    ZeroOrOne = 0x1000,
    ZeroOrMore = 0x2000,
    OneOrMore = 0x4000
};

static void quantifyTrivialAtom(TrivialAtom& trivialAtom, AtomQuantifier quantifier)
{
    ASSERT(trivialAtom & (hasNonCharacterMask | characterMask));
    ASSERT(!(trivialAtom & 0xf000));
    trivialAtom |= static_cast<uint16_t>(quantifier);
}

static AtomQuantifier trivialAtomQuantifier(TrivialAtom trivialAtom)
{
    switch (trivialAtom & 0xf000) {
    case static_cast<unsigned>(AtomQuantifier::One):
        return AtomQuantifier::One;
    case static_cast<unsigned>(AtomQuantifier::ZeroOrOne):
        return AtomQuantifier::ZeroOrOne;
    case static_cast<unsigned>(AtomQuantifier::ZeroOrMore):
        return AtomQuantifier::ZeroOrMore;
    case static_cast<unsigned>(AtomQuantifier::OneOrMore):
        return AtomQuantifier::OneOrMore;
    }
    ASSERT_NOT_REACHED();
    return AtomQuantifier::One;
}

static TrivialAtom trivialAtomForNewlineClassIDBuiltin()
{
    return hasNonCharacterMask | newlineClassIDBuiltinMask;
}

class GraphBuilder {
    struct BoundedSubGraph {
        unsigned start;
        unsigned end;
    };

public:
    GraphBuilder(NFA& nfa, PrefixTreeEntry& prefixTreeRoot, bool patternIsCaseSensitive, uint64_t patternId)
        : m_nfa(nfa)
        , m_patternIsCaseSensitive(patternIsCaseSensitive)
        , m_patternId(patternId)
        , m_activeGroup({ nfa.root(), nfa.root() })
        , m_lastPrefixTreeEntry(&prefixTreeRoot)
    {
    }

    void finalize()
    {
        if (hasError())
            return;

        sinkPendingAtomIfNecessary();

        if (m_activeGroup.start != m_activeGroup.end)
            m_nfa.setFinal(m_activeGroup.end, m_patternId);
        else
            fail(ASCIILiteral("The pattern cannot match anything."));
    }

    const String& errorMessage() const
    {
        return m_errorMessage;
    }

    void atomPatternCharacter(UChar character)
    {
        if (hasError())
            return;

        if (!isASCII(character)) {
            fail(ASCIILiteral("Only ASCII characters are supported in pattern."));
            return;
        }

        sinkPendingAtomIfNecessary();
        ASSERT(m_floatingAtomType == FloatingAtomType::Invalid);
        ASSERT(!m_pendingTrivialAtom);

        char asciiChararacter = static_cast<char>(character);
        m_pendingTrivialAtom = trivialAtomFromASCIICharacter(asciiChararacter, m_patternIsCaseSensitive);
        m_floatingAtomType = FloatingAtomType::Trivial;
    }

    void atomBuiltInCharacterClass(JSC::Yarr::BuiltInCharacterClassID builtInCharacterClassID, bool inverted)
    {
        if (hasError())
            return;

        sinkPendingAtomIfNecessary();
        ASSERT(m_floatingAtomType == FloatingAtomType::Invalid);
        ASSERT(!m_pendingTrivialAtom);

        if (builtInCharacterClassID == JSC::Yarr::NewlineClassID && inverted) {
            m_pendingTrivialAtom = trivialAtomForNewlineClassIDBuiltin();
            m_floatingAtomType = FloatingAtomType::Trivial;
        } else
            fail(ASCIILiteral("Character class is not supported."));
    }

    void quantifyAtom(unsigned minimum, unsigned maximum, bool)
    {
        if (hasError())
            return;

        switch (m_floatingAtomType) {
        case FloatingAtomType::Invalid:
            fail(ASCIILiteral("Quantifier without corresponding atom to quantify."));
            break;

        case FloatingAtomType::Trivial:
            if (!minimum && maximum == 1)
                quantifyTrivialAtom(m_pendingTrivialAtom, AtomQuantifier::ZeroOrOne);
            else if (!minimum && maximum == JSC::Yarr::quantifyInfinite)
                quantifyTrivialAtom(m_pendingTrivialAtom, AtomQuantifier::ZeroOrMore);
            else if (minimum == 1 && maximum == JSC::Yarr::quantifyInfinite)
                quantifyTrivialAtom(m_pendingTrivialAtom, AtomQuantifier::OneOrMore);
            else
                fail(ASCIILiteral("Arbitrary atom repetitions are not supported."));
            break;
        case FloatingAtomType::CharacterSet: {
            ASSERT(m_characterSetQuantifier == AtomQuantifier::One);
            if (!minimum && maximum == 1)
                m_characterSetQuantifier = AtomQuantifier::ZeroOrOne;
            else if (!minimum && maximum == JSC::Yarr::quantifyInfinite)
                m_characterSetQuantifier = AtomQuantifier::ZeroOrMore;
            else if (minimum == 1 && maximum == JSC::Yarr::quantifyInfinite)
                m_characterSetQuantifier = AtomQuantifier::OneOrMore;
            else
                fail(ASCIILiteral("Arbitrary character set repetitions are not supported."));
            break;
        }
        }
    }

    void atomBackReference(unsigned)
    {
        fail(ASCIILiteral("Patterns cannot contain backreferences."));
    }

    void assertionBOL()
    {
        fail(ASCIILiteral("Line boundary assertions are not supported yet."));
    }

    void assertionEOL()
    {
        fail(ASCIILiteral("Line boundary assertions are not supported yet."));
    }

    void assertionWordBoundary(bool)
    {
        fail(ASCIILiteral("Word boundaries assertions are not supported yet."));
    }

    void atomCharacterClassBegin(bool inverted = false)
    {
        if (hasError())
            return;

        sinkPendingAtomIfNecessary();

        ASSERT_WITH_MESSAGE(!m_pendingCharacterSet.bitCount(), "We should not have nested character classes.");
        ASSERT(m_floatingAtomType == FloatingAtomType::Invalid);

        m_buildMode = BuildMode::DirectGeneration;
        m_lastPrefixTreeEntry = nullptr;

        m_isInvertedCharacterSet = inverted;
        m_floatingAtomType = FloatingAtomType::CharacterSet;
    }

    void atomCharacterClassAtom(UChar character)
    {
        if (hasError())
            return;

        ASSERT(m_floatingAtomType == FloatingAtomType::CharacterSet);

        if (!isASCII(character)) {
            fail(ASCIILiteral("Non ASCII Character in a character set."));
            return;
        }
        m_pendingCharacterSet.set(character);
    }

    void atomCharacterClassRange(UChar a, UChar b)
    {
        if (hasError())
            return;

        ASSERT(m_floatingAtomType == FloatingAtomType::CharacterSet);

        if (!a || !b || !isASCII(a) || !isASCII(b)) {
            fail(ASCIILiteral("Non ASCII Character in a character range of a character set."));
            return;
        }
        for (unsigned i = a; i <= b; ++i)
            m_pendingCharacterSet.set(i);
    }

    void atomCharacterClassEnd()
    {
        // Nothing to do here. The character set atom may have a quantifier, we sink the atom lazily.
    }

    void atomCharacterClassBuiltIn(JSC::Yarr::BuiltInCharacterClassID, bool)
    {
        fail(ASCIILiteral("Builtins character class atoms are not supported yet."));
    }

    void atomParenthesesSubpatternBegin(bool = true)
    {
        fail(ASCIILiteral("Groups are not supported yet."));
    }

    void atomParentheticalAssertionBegin(bool = false)
    {
        fail(ASCIILiteral("Groups are not supported yet."));
    }

    void atomParenthesesEnd()
    {
        fail(ASCIILiteral("Groups are not supported yet."));
    }

    void disjunction()
    {
        fail(ASCIILiteral("Disjunctions are not supported yet."));
    }

private:
    bool hasError() const
    {
        return !m_errorMessage.isNull();
    }

    void fail(const String& errorMessage)
    {
        if (hasError())
            return;

        if (m_newPrefixSubtreeRoot)
            m_newPrefixSubtreeRoot->nextPattern.remove(m_newPrefixStaringPoint);

        m_errorMessage = errorMessage;
    }

    BoundedSubGraph sinkAtom(std::function<void(unsigned, unsigned)> transitionFunction, AtomQuantifier quantifier, unsigned start)
    {
        switch (quantifier) {
        case AtomQuantifier::One: {
            unsigned newEnd = m_nfa.createNode();
            m_nfa.addRuleId(newEnd, m_patternId);
            transitionFunction(start, newEnd);
            return { start, newEnd };
        }
        case AtomQuantifier::ZeroOrOne: {
            unsigned newEnd = m_nfa.createNode();
            m_nfa.addRuleId(newEnd, m_patternId);
            transitionFunction(start, newEnd);
            return { start, newEnd };
        }
        case AtomQuantifier::ZeroOrMore: {
            unsigned repeatStart = m_nfa.createNode();
            m_nfa.addRuleId(repeatStart, m_patternId);
            unsigned repeatEnd = m_nfa.createNode();
            m_nfa.addRuleId(repeatEnd, m_patternId);

            transitionFunction(repeatStart, repeatEnd);
            m_nfa.addEpsilonTransition(repeatEnd, repeatStart);

            m_nfa.addEpsilonTransition(start, repeatStart);

            unsigned kleenEnd = m_nfa.createNode();
            m_nfa.addRuleId(kleenEnd, m_patternId);
            m_nfa.addEpsilonTransition(repeatEnd, kleenEnd);
            m_nfa.addEpsilonTransition(start, kleenEnd);
            return { start, kleenEnd };
        }
        case AtomQuantifier::OneOrMore: {
            unsigned repeatStart = m_nfa.createNode();
            m_nfa.addRuleId(repeatStart, m_patternId);
            unsigned repeatEnd = m_nfa.createNode();
            m_nfa.addRuleId(repeatEnd, m_patternId);

            transitionFunction(repeatStart, repeatEnd);
            m_nfa.addEpsilonTransition(repeatEnd, repeatStart);

            m_nfa.addEpsilonTransition(start, repeatStart);

            unsigned afterRepeat = m_nfa.createNode();
            m_nfa.addRuleId(afterRepeat, m_patternId);
            m_nfa.addEpsilonTransition(repeatEnd, afterRepeat);
            return { start, afterRepeat };
        }
        }
    }

    void generateTransition(TrivialAtom trivialAtom, unsigned source, unsigned target)
    {
        if (trivialAtom & hasNonCharacterMask) {
            ASSERT(trivialAtom & newlineClassIDBuiltinMask);
            m_nfa.addTransitionsOnAnyCharacter(source, target);
        } else {
            if (trivialAtom & caseInsensitiveMask) {
                char character = static_cast<char>(trivialAtom & characterMask);
                m_nfa.addTransition(source, target, character);
                m_nfa.addTransition(source, target, toASCIIUpper(character));
            } else
                m_nfa.addTransition(source, target, static_cast<char>(trivialAtom & characterMask));
        }
    }

    BoundedSubGraph sinkTrivialAtom(TrivialAtom trivialAtom, unsigned start)
    {
        auto transitionFunction = [this, trivialAtom](unsigned source, unsigned target)
        {
            generateTransition(trivialAtom, source, target);
        };
        return sinkAtom(transitionFunction, trivialAtomQuantifier(trivialAtom), start);
    }

    void generateTransition(const BitVector& characterSet, bool isInverted, unsigned source, unsigned target)
    {
        ASSERT(characterSet.bitCount());
        if (!isInverted) {
            for (const auto& characterIterator : characterSet.setBits()) {
                char character = static_cast<char>(characterIterator);
                if (!m_patternIsCaseSensitive && isASCIIAlpha(character)) {
                    m_nfa.addTransition(source, target, toASCIIUpper(character));
                    m_nfa.addTransition(source, target, toASCIILower(character));
                } else
                    m_nfa.addTransition(source, target, character);
            }
        } else {
            for (unsigned i = 1; i < characterSet.size(); ++i) {
                if (characterSet.get(i))
                    continue;
                char character = static_cast<char>(i);

                if (!m_patternIsCaseSensitive && (characterSet.get(toASCIIUpper(character)) || characterSet.get(toASCIILower(character))))
                    continue;

                m_nfa.addTransition(source, target, character);
            }
        }
    }

    BoundedSubGraph sinkCharacterSet(const BitVector& characterSet, bool isInverted, unsigned start)
    {
        auto transitionFunction = [this, &characterSet, isInverted](unsigned source, unsigned target)
        {
            generateTransition(characterSet, isInverted, source, target);
        };
        return sinkAtom(transitionFunction, m_characterSetQuantifier, start);
    }

    void sinkPendingAtomIfNecessary()
    {
        if (m_floatingAtomType == FloatingAtomType::Invalid)
            return;

        switch (m_buildMode) {
        case BuildMode::PrefixTree: {
            ASSERT(m_lastPrefixTreeEntry);
            ASSERT_WITH_MESSAGE(m_floatingAtomType == FloatingAtomType::Trivial, "Only trivial atoms are handled with a prefix tree.");

            auto nextEntry = m_lastPrefixTreeEntry->nextPattern.find(m_pendingTrivialAtom);
            if (nextEntry != m_lastPrefixTreeEntry->nextPattern.end()) {
                m_lastPrefixTreeEntry = nextEntry->value.get();
                m_nfa.addRuleId(m_lastPrefixTreeEntry->nfaNode, m_patternId);
            } else {
                std::unique_ptr<PrefixTreeEntry> nextPrefixTreeEntry = std::make_unique<PrefixTreeEntry>();

                BoundedSubGraph newSubGraph = sinkTrivialAtom(m_pendingTrivialAtom, m_lastPrefixTreeEntry->nfaNode);
                nextPrefixTreeEntry->nfaNode = newSubGraph.end;

                auto addResult = m_lastPrefixTreeEntry->nextPattern.set(m_pendingTrivialAtom, WTF::move(nextPrefixTreeEntry));
                ASSERT(addResult.isNewEntry);

                if (!m_newPrefixSubtreeRoot) {
                    m_newPrefixSubtreeRoot = m_lastPrefixTreeEntry;
                    m_newPrefixStaringPoint = m_pendingTrivialAtom;
                }

                m_lastPrefixTreeEntry = addResult.iterator->value.get();
            }
            m_activeGroup.end = m_lastPrefixTreeEntry->nfaNode;
            ASSERT(m_lastPrefixTreeEntry);
            break;
        }
        case BuildMode::DirectGeneration: {
            ASSERT(!m_lastPrefixTreeEntry);

            switch (m_floatingAtomType) {
            case FloatingAtomType::Invalid:
                ASSERT_NOT_REACHED();
                break;
            case FloatingAtomType::Trivial: {
                BoundedSubGraph newSubGraph = sinkTrivialAtom(m_pendingTrivialAtom, m_activeGroup.end);
                m_activeGroup.end = newSubGraph.end;
                break;
            }
            case FloatingAtomType::CharacterSet:
                if (!m_pendingCharacterSet.bitCount()) {
                    fail(ASCIILiteral("Empty character set."));
                    return;
                }
                BoundedSubGraph newSubGraph = sinkCharacterSet(m_pendingCharacterSet, m_isInvertedCharacterSet, m_activeGroup.end);
                m_activeGroup.end = newSubGraph.end;

                m_isInvertedCharacterSet = false;
                m_characterSetQuantifier = AtomQuantifier::One;
                m_pendingCharacterSet.clearAll();
                break;
            }
            break;
            }
        }

        m_pendingTrivialAtom = 0;
        m_floatingAtomType = FloatingAtomType::Invalid;
    }

    NFA& m_nfa;
    bool m_patternIsCaseSensitive;
    const uint64_t m_patternId;

    BoundedSubGraph m_activeGroup;

    PrefixTreeEntry* m_lastPrefixTreeEntry;
    enum class FloatingAtomType {
        Invalid,
        Trivial,
        CharacterSet
    };
    FloatingAtomType m_floatingAtomType { FloatingAtomType::Invalid };
    TrivialAtom m_pendingTrivialAtom = 0;

    bool m_isInvertedCharacterSet { false };
    BitVector m_pendingCharacterSet { 128 };
    AtomQuantifier m_characterSetQuantifier { AtomQuantifier::One };

    enum class BuildMode {
        PrefixTree,
        DirectGeneration
    };
    BuildMode m_buildMode { BuildMode::PrefixTree };

    PrefixTreeEntry* m_newPrefixSubtreeRoot = nullptr;
    TrivialAtom m_newPrefixStaringPoint = 0;

    String m_errorMessage;
};

URLFilterParser::URLFilterParser(NFA& nfa)
    : m_nfa(nfa)
{
    m_prefixTreeRoot.nfaNode = nfa.root();
}

String URLFilterParser::addPattern(const String& pattern, bool patternIsCaseSensitive, uint64_t patternId)
{
    if (!pattern.containsOnlyASCII())
        return ASCIILiteral("URLFilterParser only supports ASCII patterns.");
    ASSERT(!pattern.isEmpty());

    if (pattern.isEmpty())
        return ASCIILiteral("Empty pattern.");

    unsigned oldSize = m_nfa.graphSize();

    String error;

    GraphBuilder graphBuilder(m_nfa, m_prefixTreeRoot, patternIsCaseSensitive, patternId);
    error = String(JSC::Yarr::parse(graphBuilder, pattern, 0));
    if (error.isNull())
        graphBuilder.finalize();

    if (error.isNull())
        error = graphBuilder.errorMessage();

    if (!error.isNull())
        m_nfa.restoreToGraphSize(oldSize);

    return error;
}

} // namespace ContentExtensions
} // namespace WebCore

#endif // ENABLE(CONTENT_EXTENSIONS)
