/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "CombinedURLFilters.h"

#if ENABLE(CONTENT_EXTENSIONS)

#include "Term.h"
#include <wtf/DataLog.h>
#include <wtf/Vector.h>
#include <wtf/text/CString.h>

namespace WebCore {

namespace ContentExtensions {

struct PrefixTreeEdge {
    Term term;
    std::unique_ptr<PrefixTreeVertex> child;
};
    
typedef Vector<PrefixTreeEdge, 0, WTF::CrashOnOverflow, 1> PrefixTreeEdges;

struct PrefixTreeVertex {
    PrefixTreeEdges edges;
    ActionList finalActions;
};

#if CONTENT_EXTENSIONS_PERFORMANCE_REPORTING
static size_t recursiveMemoryUsed(const PrefixTreeVertex& vertex)
{
    size_t size = sizeof(PrefixTreeVertex)
        + vertex.edges.capacity() * sizeof(PrefixTreeEdge)
        + vertex.finalActions.capacity() * sizeof(uint64_t);
    for (const auto& edge : vertex.edges) {
        ASSERT(edge.child);
        size += recursiveMemoryUsed(*edge.child.get());
    }
    return size;
}

size_t CombinedURLFilters::memoryUsed() const
{
    ASSERT(m_prefixTreeRoot);
    return recursiveMemoryUsed(*m_prefixTreeRoot.get());
}
#endif
    
#if CONTENT_EXTENSIONS_STATE_MACHINE_DEBUGGING
static String prefixTreeVertexToString(const PrefixTreeVertex& vertex, unsigned depth)
{
    StringBuilder builder;
    while (depth--)
        builder.append("  ");
    builder.append("vertex actions: ");
    for (auto action : vertex.finalActions) {
        builder.appendNumber(action);
        builder.append(',');
    }
    builder.append('\n');
    return builder.toString();
}

static void recursivePrint(const PrefixTreeVertex& vertex, unsigned depth)
{
    dataLogF("%s", prefixTreeVertexToString(vertex, depth).utf8().data());
    for (const auto& edge : vertex.edges) {
        StringBuilder builder;
        for (unsigned i = 0; i < depth * 2; ++i)
            builder.append(' ');
        builder.append("vertex edge: ");
        builder.append(edge.term.toString());
        builder.append('\n');
        dataLogF("%s", builder.toString().utf8().data());
        ASSERT(edge.child);
        recursivePrint(*edge.child.get(), depth + 1);
    }
}

void CombinedURLFilters::print() const
{
    recursivePrint(*m_prefixTreeRoot.get(), 0);
}
#endif

CombinedURLFilters::CombinedURLFilters()
    : m_prefixTreeRoot(std::make_unique<PrefixTreeVertex>())
{
}

CombinedURLFilters::~CombinedURLFilters()
{
}

bool CombinedURLFilters::isEmpty() const
{
    return m_prefixTreeRoot->edges.isEmpty();
}

void CombinedURLFilters::addDomain(uint64_t actionId, const String& domain)
{
    unsigned domainLength = domain.length();
    if (domainLength && domain[0] == '*') {
        // If domain starts with a '*' then it means match domain and its subdomains, like (^|.)domain$
        // This way a domain of "*webkit.org" will match "bugs.webkit.org" and "webkit.org".
        Vector<Term> prependDot;
        Vector<Term> prependBeginningOfLine;
        prependDot.reserveInitialCapacity(domainLength + 2);
        prependBeginningOfLine.reserveInitialCapacity(domainLength); // This is just no .* at the beginning.
        
        Term canonicalDotStar(Term::UniversalTransition);
        canonicalDotStar.quantify(AtomQuantifier::ZeroOrMore);
        prependDot.uncheckedAppend(canonicalDotStar);
        prependDot.uncheckedAppend(Term('.', true));
        
        for (unsigned i = 1; i < domainLength; i++) {
            ASSERT(isASCII(domain[i]));
            ASSERT(!isASCIIUpper(domain[i]));
            prependDot.uncheckedAppend(Term(domain[i], true));
            prependBeginningOfLine.uncheckedAppend(Term(domain[i], true));
        }
        prependDot.uncheckedAppend(Term::EndOfLineAssertionTerm);
        prependBeginningOfLine.uncheckedAppend(Term::EndOfLineAssertionTerm);
        
        addPattern(actionId, prependDot);
        addPattern(actionId, prependBeginningOfLine);
    } else {
        // This is like adding ^domain$, but interpreting domain as a series of characters, not a regular expression.
        // "webkit.org" will match "webkit.org" but not "bugs.webkit.org".
        Vector<Term> prependBeginningOfLine;
        prependBeginningOfLine.reserveInitialCapacity(domainLength + 1); // This is just no .* at the beginning.
        for (unsigned i = 0; i < domainLength; i++) {
            ASSERT(isASCII(domain[i]));
            ASSERT(!isASCIIUpper(domain[i]));
            prependBeginningOfLine.uncheckedAppend(Term(domain[i], true));
        }
        prependBeginningOfLine.uncheckedAppend(Term::EndOfLineAssertionTerm);
        addPattern(actionId, prependBeginningOfLine);
    }
}

void CombinedURLFilters::addPattern(uint64_t actionId, const Vector<Term>& pattern)
{
    ASSERT_WITH_MESSAGE(!pattern.isEmpty(), "The parser should have excluded empty patterns before reaching CombinedURLFilters.");

    if (pattern.isEmpty())
        return;

    // Extend the prefix tree with the new pattern.
    PrefixTreeVertex* lastPrefixTree = m_prefixTreeRoot.get();

    for (const Term& term : pattern) {
        size_t nextEntryIndex = WTF::notFound;
        for (size_t i = 0; i < lastPrefixTree->edges.size(); ++i) {
            if (lastPrefixTree->edges[i].term == term) {
                nextEntryIndex = i;
                break;
            }
        }
        if (nextEntryIndex != WTF::notFound)
            lastPrefixTree = lastPrefixTree->edges[nextEntryIndex].child.get();
        else {
            lastPrefixTree->edges.append(PrefixTreeEdge({term, std::make_unique<PrefixTreeVertex>()}));
            lastPrefixTree = lastPrefixTree->edges.last().child.get();
        }
    }

    ActionList& actions = lastPrefixTree->finalActions;
    if (actions.find(actionId) == WTF::notFound)
        actions.append(actionId);
}

static void generateNFAForSubtree(NFA& nfa, ImmutableCharNFANodeBuilder&& subtreeRoot, PrefixTreeVertex& root, size_t maxNFASize)
{
    // This recurses the subtree of the prefix tree.
    // For each edge that has fixed length (no quantifiers like ?, *, or +) it generates the nfa graph,
    // recurses into children, and deletes any processed leaf nodes.
    struct ActiveSubtree {
        ActiveSubtree(PrefixTreeVertex& vertex, ImmutableCharNFANodeBuilder&& nfaNode, unsigned edgeIndex)
            : vertex(vertex)
            , nfaNode(WTF::move(nfaNode))
            , edgeIndex(edgeIndex)
        {
        }
        PrefixTreeVertex& vertex;
        ImmutableCharNFANodeBuilder nfaNode;
        unsigned edgeIndex;
    };
    Vector<ActiveSubtree> stack;
    if (!root.edges.isEmpty())
        stack.append(ActiveSubtree(root, WTF::move(subtreeRoot), 0));
    bool nfaTooBig = false;
    
    // Generate graphs for each subtree that does not contain any quantifiers.
    while (!stack.isEmpty()) {
        PrefixTreeVertex& vertex = stack.last().vertex;
        const unsigned edgeIndex = stack.last().edgeIndex;
        
        // Only stop generating an NFA at a leaf to ensure we have a correct NFA. We could go slightly over the maxNFASize.
        if (vertex.edges.isEmpty() && nfa.nodes.size() > maxNFASize)
            nfaTooBig = true;
        
        if (edgeIndex < vertex.edges.size()) {
            auto& edge = vertex.edges[edgeIndex];
            
            // Clean up any processed leaves and return early if we are past the maxNFASize.
            if (nfaTooBig) {
                stack.last().edgeIndex = stack.last().vertex.edges.size();
                continue;
            }
            
            // Quantified edges in the subtree will be a part of another NFA.
            if (!edge.term.hasFixedLength()) {
                stack.last().edgeIndex++;
                continue;
            }

            ImmutableCharNFANodeBuilder newNode = edge.term.generateGraph(nfa, stack.last().nfaNode, edge.child->finalActions);
            ASSERT(edge.child.get());
            stack.append(ActiveSubtree(*edge.child.get(), WTF::move(newNode), 0));
        } else {
            ASSERT(edgeIndex == vertex.edges.size());
            vertex.edges.removeAllMatching([](PrefixTreeEdge& edge)
            {
                return edge.term.isDeletedValue();
            });
            stack.removeLast();
            if (!stack.isEmpty()) {
                auto& activeSubtree = stack.last();
                auto& edge = activeSubtree.vertex.edges[stack.last().edgeIndex];
                if (edge.child->edges.isEmpty())
                    edge.term = Term(Term::DeletedValue); // Mark this leaf for deleting.
                activeSubtree.edgeIndex++;
            }
        }
    }
}

void CombinedURLFilters::processNFAs(size_t maxNFASize, std::function<void(NFA&&)> handler)
{
#if CONTENT_EXTENSIONS_STATE_MACHINE_DEBUGGING
    print();
#endif
    while (true) {
        // Traverse out to a leaf.
        Vector<PrefixTreeVertex*, 128> stack;
        PrefixTreeVertex* vertex = m_prefixTreeRoot.get();
        while (true) {
            ASSERT(vertex);
            stack.append(vertex);
            if (vertex->edges.isEmpty())
                break;
            vertex = vertex->edges.last().child.get();
        }
        if (stack.size() == 1)
            break; // We're done once we have processed and removed all the edges in the prefix tree.
        
        // Find the prefix root for this NFA. This is the vertex after the last term with a quantifier if there is one,
        // or the root if there are no quantifiers left.
        while (stack.size() > 1) {
            if (!stack[stack.size() - 2]->edges.last().term.hasFixedLength())
                break;
            stack.removeLast();
        }
        ASSERT_WITH_MESSAGE(!stack.isEmpty(), "At least the root should be in the stack");

        // Make an NFA with the subtrees for whom this is also the last quantifier (or who also have no quantifier).
        NFA nfa;
        {
            // Put the prefix into the NFA.
            ImmutableCharNFANodeBuilder lastNode(nfa);
            for (unsigned i = 0; i < stack.size() - 1; ++i) {
                const PrefixTreeEdge& edge = stack[i]->edges.last();
                ImmutableCharNFANodeBuilder newNode = edge.term.generateGraph(nfa, lastNode, edge.child->finalActions);
                lastNode = WTF::move(newNode);
            }

            // Put the non-quantified vertices in the subtree into the NFA and delete them.
            ASSERT(stack.last());
            generateNFAForSubtree(nfa, WTF::move(lastNode), *stack.last(), maxNFASize);
        }
        nfa.finalize();

        handler(WTF::move(nfa));

        // Clean up any processed leaf nodes.
        while (true) {
            if (stack.size() > 1) {
                if (stack[stack.size() - 1]->edges.isEmpty()) {
                    stack[stack.size() - 2]->edges.removeLast();
                    stack.removeLast();
                } else
                    break; // Vertex is not a leaf.
            } else
                break; // Leave the empty root.
        }
    }
}

} // namespace ContentExtensions
} // namespace WebCore

#endif // ENABLE(CONTENT_EXTENSIONS)
