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

enum class TrivialAtomQuantifier : uint16_t {
    ZeroOrOne = 0x1000,
    ZeroToMany = 0x2000,
    OneToMany = 0x4000
};

static void quantifyTrivialAtom(TrivialAtom& trivialAtom, TrivialAtomQuantifier quantifier)
{
    ASSERT(trivialAtom & (hasNonCharacterMask | characterMask));
    ASSERT(!(trivialAtom & 0xf000));
    trivialAtom |= static_cast<uint16_t>(quantifier);
}

static TrivialAtom trivialAtomForNewlineClassIDBuiltin()
{
    return hasNonCharacterMask | newlineClassIDBuiltinMask;
}

class GraphBuilder {
private:
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
        if (!isASCII(character)) {
            fail(ASCIILiteral("Only ASCII characters are supported in pattern."));
            return;
        }

        if (hasError())
            return;

        sinkPendingAtomIfNecessary();

        char asciiChararacter = static_cast<char>(character);
        m_hasValidAtom = true;

        ASSERT(m_lastPrefixTreeEntry);
        m_pendingTrivialAtom = trivialAtomFromASCIICharacter(asciiChararacter, m_patternIsCaseSensitive);
    }

    void atomBuiltInCharacterClass(JSC::Yarr::BuiltInCharacterClassID builtInCharacterClassID, bool inverted)
    {
        if (hasError())
            return;

        if (builtInCharacterClassID == JSC::Yarr::NewlineClassID && inverted) {
            m_hasValidAtom = true;
            ASSERT(m_lastPrefixTreeEntry);
            m_pendingTrivialAtom = trivialAtomForNewlineClassIDBuiltin();
        } else
            fail(ASCIILiteral("Character class is not supported."));
    }

    void quantifyAtom(unsigned minimum, unsigned maximum, bool)
    {
        if (hasError())
            return;

        ASSERT(m_hasValidAtom);
        if (!m_hasValidAtom) {
            fail(ASCIILiteral("Quantifier without corresponding atom to quantify."));
            return;
        }

        ASSERT(m_lastPrefixTreeEntry);
        if (!minimum && maximum == 1)
            quantifyTrivialAtom(m_pendingTrivialAtom, TrivialAtomQuantifier::ZeroOrOne);
        else if (!minimum && maximum == JSC::Yarr::quantifyInfinite)
            quantifyTrivialAtom(m_pendingTrivialAtom, TrivialAtomQuantifier::ZeroToMany);
        else if (minimum == 1 && maximum == JSC::Yarr::quantifyInfinite)
            quantifyTrivialAtom(m_pendingTrivialAtom, TrivialAtomQuantifier::OneToMany);
        else
            fail(ASCIILiteral("Arbitrary atom repetitions are not supported."));
    }

    NO_RETURN_DUE_TO_ASSERT void atomBackReference(unsigned)
    {
        fail(ASCIILiteral("Patterns cannot contain backreferences."));
        ASSERT_NOT_REACHED();
    }

    void atomCharacterClassAtom(UChar)
    {
        fail(ASCIILiteral("Character class atoms are not supported yet."));
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

    void atomCharacterClassBegin(bool = false)
    {
        fail(ASCIILiteral("Character class atoms are not supported yet."));
    }

    void atomCharacterClassRange(UChar, UChar)
    {
        fail(ASCIILiteral("Character class ranges are not supported yet."));
    }

    void atomCharacterClassBuiltIn(JSC::Yarr::BuiltInCharacterClassID, bool)
    {
        fail(ASCIILiteral("Buildins character class atoms are not supported yet."));
    }

    void atomCharacterClassEnd()
    {
        fail(ASCIILiteral("Character class are not supported yet."));
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

    void generateTransition(TrivialAtom trivialAtom, unsigned source, unsigned target)
    {
        if (trivialAtom & hasNonCharacterMask) {
            ASSERT(trivialAtom & newlineClassIDBuiltinMask);
            for (unsigned i = 1; i < 128; ++i)
                m_nfa.addTransition(source, target, i);
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
        if (trivialAtom & static_cast<uint16_t>(TrivialAtomQuantifier::ZeroOrOne)) {
            unsigned newEnd = m_nfa.createNode();
            m_nfa.addRuleId(newEnd, m_patternId);
            generateTransition(trivialAtom, start, newEnd);
            m_nfa.addEpsilonTransition(start, newEnd);
            return { start, newEnd };
        }

        if (trivialAtom & static_cast<uint16_t>(TrivialAtomQuantifier::ZeroToMany)) {
            unsigned repeatStart = m_nfa.createNode();
            m_nfa.addRuleId(repeatStart, m_patternId);
            unsigned repeatEnd = m_nfa.createNode();
            m_nfa.addRuleId(repeatEnd, m_patternId);

            generateTransition(trivialAtom, repeatStart, repeatEnd);
            m_nfa.addEpsilonTransition(repeatEnd, repeatStart);

            m_nfa.addEpsilonTransition(start, repeatStart);

            unsigned kleenEnd = m_nfa.createNode();
            m_nfa.addRuleId(kleenEnd, m_patternId);
            m_nfa.addEpsilonTransition(repeatEnd, kleenEnd);
            m_nfa.addEpsilonTransition(start, kleenEnd);
            return { start, kleenEnd };
        }

        if (trivialAtom & static_cast<uint16_t>(TrivialAtomQuantifier::OneToMany)) {
            unsigned repeatStart = m_nfa.createNode();
            m_nfa.addRuleId(repeatStart, m_patternId);
            unsigned repeatEnd = m_nfa.createNode();
            m_nfa.addRuleId(repeatEnd, m_patternId);

            generateTransition(trivialAtom, repeatStart, repeatEnd);
            m_nfa.addEpsilonTransition(repeatEnd, repeatStart);

            m_nfa.addEpsilonTransition(start, repeatStart);

            unsigned afterRepeat = m_nfa.createNode();
            m_nfa.addRuleId(afterRepeat, m_patternId);
            m_nfa.addEpsilonTransition(repeatEnd, afterRepeat);
            return { start, afterRepeat };
        }

        unsigned newEnd = m_nfa.createNode();
        m_nfa.addRuleId(newEnd, m_patternId);
        generateTransition(trivialAtom, start, newEnd);
        return { start, newEnd };
    }

    void sinkPendingAtomIfNecessary()
    {
        ASSERT(m_lastPrefixTreeEntry);

        if (!m_hasValidAtom)
            return;

        ASSERT(m_pendingTrivialAtom);

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

            m_newPrefixSubtreeRoot = m_lastPrefixTreeEntry;
            m_newPrefixStaringPoint = m_pendingTrivialAtom;

            m_lastPrefixTreeEntry = addResult.iterator->value.get();
        }
        ASSERT(m_lastPrefixTreeEntry);

        m_activeGroup.end = m_lastPrefixTreeEntry->nfaNode;
        m_pendingTrivialAtom = 0;
        m_hasValidAtom = false;
    }

    NFA& m_nfa;
    bool m_patternIsCaseSensitive;
    const uint64_t m_patternId;

    BoundedSubGraph m_activeGroup;

    PrefixTreeEntry* m_lastPrefixTreeEntry;
    bool m_hasValidAtom = false;
    TrivialAtom m_pendingTrivialAtom = 0;

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
