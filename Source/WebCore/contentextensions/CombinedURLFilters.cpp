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
    bool inVariableLengthPrefix { false };
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

void CombinedURLFilters::clear()
{
    m_prefixTreeRoot = std::make_unique<PrefixTreeVertex>();
}

void CombinedURLFilters::addPattern(uint64_t actionId, const Vector<Term>& pattern)
{
    ASSERT_WITH_MESSAGE(!pattern.isEmpty(), "The parser should have excluded empty patterns before reaching CombinedURLFilters.");

    if (pattern.isEmpty())
        return;

    Vector<PrefixTreeVertex*, 128> prefixTreeVerticesForPattern;
    prefixTreeVerticesForPattern.reserveInitialCapacity(pattern.size() + 1);

    // Extend the prefix tree with the new pattern.
    bool hasNewTerm = false;
    PrefixTreeVertex* lastPrefixTree = m_prefixTreeRoot.get();
    prefixTreeVerticesForPattern.append(lastPrefixTree);

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
            hasNewTerm = true;

            lastPrefixTree->edges.append(PrefixTreeEdge({term, std::make_unique<PrefixTreeVertex>()}));
            lastPrefixTree = lastPrefixTree->edges.last().child.get();
        }
        prefixTreeVerticesForPattern.append(lastPrefixTree);
    }

    ActionList& actions = prefixTreeVerticesForPattern.last()->finalActions;
    if (actions.find(actionId) == WTF::notFound)
        actions.append(actionId);

    if (!hasNewTerm)
        return;

    bool hasSeenVariableLengthTerms = false;
    for (unsigned i = pattern.size(); i--;) {
        const Term& term = pattern[i];
        hasSeenVariableLengthTerms |= !term.hasFixedLength();
        prefixTreeVerticesForPattern[i + 1]->inVariableLengthPrefix |= hasSeenVariableLengthTerms;
    }
    prefixTreeVerticesForPattern[0]->inVariableLengthPrefix |= hasSeenVariableLengthTerms;
}

struct ActiveSubtree {
    const PrefixTreeVertex* vertex;
    PrefixTreeEdges::const_iterator iterator;
};

static void generateNFAForSubtree(NFA& nfa, unsigned rootId, const PrefixTreeVertex& prefixTreeVertex)
{
    ASSERT_WITH_MESSAGE(!prefixTreeVertex.inVariableLengthPrefix, "This code assumes the subtrees with variable prefix length have already been handled.");

    struct ActiveNFASubtree : ActiveSubtree {
        ActiveNFASubtree(const PrefixTreeVertex* vertex, PrefixTreeEdges::const_iterator iterator, unsigned nodeIndex)
            : ActiveSubtree({ vertex, iterator })
            , lastNodeIndex(nodeIndex)
        {
        }
        unsigned lastNodeIndex;
    };

    Vector<ActiveNFASubtree> activeStack;
    activeStack.append(ActiveNFASubtree(&prefixTreeVertex, prefixTreeVertex.edges.begin(), rootId));

    while (true) {
    ProcessSubtree:
        for (ActiveNFASubtree& activeSubtree = activeStack.last(); activeSubtree.iterator != activeSubtree.vertex->edges.end(); ++activeSubtree.iterator) {
            if (activeSubtree.iterator->child->inVariableLengthPrefix)
                continue;

            const Term& term = activeSubtree.iterator->term;
            unsigned newEndNodeIndex = term.generateGraph(nfa, activeSubtree.lastNodeIndex, activeSubtree.iterator->child->finalActions);

            PrefixTreeVertex* prefixTreeVertex = activeSubtree.iterator->child.get();
            if (!prefixTreeVertex->edges.isEmpty()) {
                activeStack.append(ActiveNFASubtree(prefixTreeVertex, prefixTreeVertex->edges.begin(), newEndNodeIndex));
                goto ProcessSubtree;
            }
        }

        activeStack.removeLast();
        if (activeStack.isEmpty())
            break;
        ++activeStack.last().iterator;
    }
}

void CombinedURLFilters::processNFAs(std::function<void(NFA&&)> handler) const
{
    Vector<ActiveSubtree> activeStack;
    activeStack.append(ActiveSubtree({ m_prefixTreeRoot.get(), m_prefixTreeRoot->edges.begin() }));

    while (true) {
    ProcessSubtree:
        ActiveSubtree& activeSubtree = activeStack.last();

        // We go depth first into the subtrees with variable prefix. Find the next subtree.
        for (; activeSubtree.iterator != activeSubtree.vertex->edges.end(); ++activeSubtree.iterator) {
            PrefixTreeVertex* prefixTreeVertex = activeSubtree.iterator->child.get();
            if (prefixTreeVertex->inVariableLengthPrefix) {
                activeStack.append(ActiveSubtree({ prefixTreeVertex, prefixTreeVertex->edges.begin() }));
                goto ProcessSubtree;
            }
        }

        // After we reached here, we know that all the subtrees with variable prefixes have been processed,
        // time to generate the NFA for the graph rooted here.
        bool needToGenerate = activeSubtree.vertex->edges.isEmpty() && !activeSubtree.vertex->finalActions.isEmpty();
        if (!needToGenerate) {
            for (const auto& edge : activeSubtree.vertex->edges) {
                if (!edge.child->inVariableLengthPrefix) {
                    needToGenerate = true;
                    break;
                }
            }
        }

        if (needToGenerate) {
            NFA nfa;

            unsigned prefixEnd = nfa.root();

            for (unsigned i = 0; i < activeStack.size() - 1; ++i) {
                const Term& term = activeStack[i].iterator->term;
                prefixEnd = term.generateGraph(nfa, prefixEnd, activeStack[i].iterator->child->finalActions);
            }

            for (const auto& edge : activeSubtree.vertex->edges) {
                if (!edge.child->inVariableLengthPrefix) {
                    unsigned newSubtreeStart = edge.term.generateGraph(nfa, prefixEnd, edge.child->finalActions);
                    generateNFAForSubtree(nfa, newSubtreeStart, *edge.child);
                }
            }
            
            handler(WTF::move(nfa));
        }

        // We have processed all the subtrees of this level, pop the stack and move on to the next sibling.
        activeStack.removeLast();
        if (activeStack.isEmpty())
            break;
        ++activeStack.last().iterator;
    }
}

} // namespace ContentExtensions
} // namespace WebCore

#endif // ENABLE(CONTENT_EXTENSIONS)
