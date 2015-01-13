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

class GraphBuilder {
private:
    struct BoundedSubGraph {
        unsigned start;
        unsigned end;
    };
public:
    GraphBuilder(NFA& nfa, uint64_t patternId)
        : m_nfa(nfa)
        , m_patternId(patternId)
        , m_activeGroup({ nfa.root(), nfa.root() })
        , m_lastAtom(m_activeGroup)
    {
    }

    void finalize()
    {
        if (hasError())
            return;
        if (m_activeGroup.start != m_activeGroup.end)
            m_nfa.setFinal(m_activeGroup.end);
        else
            fail(ASCIILiteral("The pattern cannot match anything."));
    }

    const String& errorMessage() const
    {
        return m_errorMessage;
    }

    void atomPatternCharacter(UChar character)
    {
        if (isASCII(character)) {
            fail(ASCIILiteral("Only ASCII characters are supported in pattern."));
            return;
        }

        if (hasError())
            return;

        m_hasValidAtom = true;
        unsigned newEnd = m_nfa.createNode(m_patternId);
        m_nfa.addTransition(m_lastAtom.end, newEnd, static_cast<char>(character));
        m_lastAtom.start = m_lastAtom.end;
        m_lastAtom.end = newEnd;
        m_activeGroup.end = m_lastAtom.end;
    }

    void atomBuiltInCharacterClass(JSC::Yarr::BuiltInCharacterClassID builtInCharacterClassID, bool inverted)
    {
        if (hasError())
            return;

        if (builtInCharacterClassID == JSC::Yarr::NewlineClassID && inverted) {
            // FIXME: handle new line properly.
            m_hasValidAtom = true;
            unsigned newEnd = m_nfa.createNode(m_patternId);
            for (unsigned i = 1; i < 128; ++i)
                m_nfa.addTransition(m_lastAtom.end, newEnd, i);
            m_lastAtom.start = m_lastAtom.end;
            m_lastAtom.end = newEnd;
            m_activeGroup.end = m_lastAtom.end;
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

        if (!minimum && maximum == 1)
            m_nfa.addEpsilonTransition(m_lastAtom.start, m_lastAtom.end);
        else if (!minimum && maximum == JSC::Yarr::quantifyInfinite) {
            m_nfa.addEpsilonTransition(m_lastAtom.start, m_lastAtom.end);
            m_nfa.addEpsilonTransition(m_lastAtom.end, m_lastAtom.start);
        } else if (minimum == 1 && maximum == JSC::Yarr::quantifyInfinite)
            m_nfa.addEpsilonTransition(m_lastAtom.end, m_lastAtom.start);
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
        m_errorMessage = errorMessage;
    }

    NFA& m_nfa;
    const uint64_t m_patternId;

    BoundedSubGraph m_activeGroup;

    bool m_hasValidAtom = false;
    BoundedSubGraph m_lastAtom;

    String m_errorMessage;
};

void URLFilterParser::parse(const String& pattern, uint64_t patternId, NFA& nfa)
{
    if (!pattern.containsOnlyASCII())
        m_errorMessage = ASCIILiteral("URLFilterParser only supports ASCII patterns.");
    ASSERT(!pattern.isEmpty());

    if (pattern.isEmpty())
        return;

    unsigned oldSize = nfa.graphSize();

    GraphBuilder graphBuilder(nfa, patternId);
    const char* error = JSC::Yarr::parse(graphBuilder, pattern, 0);
    if (error)
        m_errorMessage = String(error);
    else
        graphBuilder.finalize();

    if (!error)
        m_errorMessage = graphBuilder.errorMessage();

    if (hasError())
        nfa.restoreToGraphSize(oldSize);
}

} // namespace ContentExtensions
} // namespace WebCore

#endif // ENABLE(CONTENT_EXTENSIONS)
