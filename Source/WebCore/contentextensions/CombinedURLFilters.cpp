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
#include <wtf/Vector.h>

namespace WebCore {

namespace ContentExtensions {

typedef Vector<std::pair<Term, std::unique_ptr<PrefixTreeVertex>>, 0, WTF::CrashOnOverflow, 1> PrefixTreeEdges;

struct PrefixTreeVertex {
    PrefixTreeEdges edges;
    ActionList finalActions;
    bool inVariableLengthPrefix { false };
};

static size_t recursiveMemoryUsed(const std::unique_ptr<PrefixTreeVertex>& node)
{
    size_t size = sizeof(PrefixTreeVertex)
        + node->edges.capacity() * sizeof(std::pair<Term, std::unique_ptr<PrefixTreeVertex>>)
        + node->finalActions.capacity() * sizeof(uint64_t);
    for (const auto& child : node->edges)
        size += recursiveMemoryUsed(child.second);
    return size;
}

size_t CombinedURLFilters::memoryUsed() const
{
    return recursiveMemoryUsed(m_prefixTreeRoot);
}
    
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
            if (lastPrefixTree->edges[i].first == term) {
                nextEntryIndex = i;
                break;
            }
        }
        if (nextEntryIndex != WTF::notFound)
            lastPrefixTree = lastPrefixTree->edges[nextEntryIndex].second.get();
        else {
            hasNewTerm = true;

            std::unique_ptr<PrefixTreeVertex> nextPrefixTreeVertex = std::make_unique<PrefixTreeVertex>();

            ASSERT(lastPrefixTree->edges.find(std::make_pair(term, std::make_unique<PrefixTreeVertex>())) == WTF::notFound);
            lastPrefixTree->edges.append(std::make_pair(term, WTF::move(nextPrefixTreeVertex)));
            lastPrefixTree = lastPrefixTree->edges.last().second.get();
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
            if (activeSubtree.iterator->second->inVariableLengthPrefix)
                continue;

            const Term& term = activeSubtree.iterator->first;
            unsigned newEndNodeIndex = term.generateGraph(nfa, activeSubtree.lastNodeIndex, activeSubtree.iterator->second->finalActions);

            PrefixTreeVertex* prefixTreeVertex = activeSubtree.iterator->second.get();
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
            PrefixTreeVertex* prefixTreeVertex = activeSubtree.iterator->second.get();
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
                if (!edge.second->inVariableLengthPrefix) {
                    needToGenerate = true;
                    break;
                }
            }
        }

        if (needToGenerate) {
            NFA nfa;

            unsigned prefixEnd = nfa.root();

            for (unsigned i = 0; i < activeStack.size() - 1; ++i) {
                const Term& term = activeStack[i].iterator->first;
                prefixEnd = term.generateGraph(nfa, prefixEnd, activeStack[i].iterator->second->finalActions);
            }

            for (const auto& edge : activeSubtree.vertex->edges) {
                if (!edge.second->inVariableLengthPrefix) {
                    const Term& term = edge.first;
                    unsigned newSubtreeStart = term.generateGraph(nfa, prefixEnd, edge.second->finalActions);
                    generateNFAForSubtree(nfa, newSubtreeStart, *edge.second);
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
