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

#include "HashableActionList.h"
#include "Term.h"
#include <wtf/DataLog.h>
#include <wtf/Vector.h>
#include <wtf/text/CString.h>

namespace WebCore {

namespace ContentExtensions {

struct PrefixTreeEdge {
    const Term* term;
    std::unique_ptr<PrefixTreeVertex> child;
};
    
typedef Vector<PrefixTreeEdge, 0, WTF::CrashOnOverflow, 1> PrefixTreeEdges;

struct PrefixTreeVertex {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;

    PrefixTreeEdges edges;
};

struct ReverseSuffixTreeVertex;
struct ReverseSuffixTreeEdge {
    const Term* term;
    std::unique_ptr<ReverseSuffixTreeVertex> child;
};
typedef Vector<ReverseSuffixTreeEdge, 0, WTF::CrashOnOverflow, 1> ReverseSuffixTreeEdges;

struct ReverseSuffixTreeVertex {
    ReverseSuffixTreeEdges edges;
    uint32_t nodeId;
};
typedef HashMap<HashableActionList, ReverseSuffixTreeVertex, HashableActionListHash, HashableActionListHashTraits> ReverseSuffixTreeRoots;

#if CONTENT_EXTENSIONS_PERFORMANCE_REPORTING
static size_t recursiveMemoryUsed(const PrefixTreeVertex& vertex)
{
    size_t size = sizeof(PrefixTreeVertex)
        + vertex.edges.capacity() * sizeof(PrefixTreeEdge);
    for (const auto& edge : vertex.edges) {
        ASSERT(edge.child);
        size += recursiveMemoryUsed(*edge.child.get());
    }
    return size;
}

size_t CombinedURLFilters::memoryUsed() const
{
    ASSERT(m_prefixTreeRoot);

    size_t actionsSize = 0;
    for (const auto& slot : m_actions)
        actionsSize += slot.value.capacity() * sizeof(uint64_t);

    return sizeof(CombinedURLFilters)
        + m_alphabet.memoryUsed()
        + recursiveMemoryUsed(*m_prefixTreeRoot.get())
        + sizeof(HashMap<PrefixTreeVertex*, ActionList>)
        + m_actions.capacity() * (sizeof(PrefixTreeVertex*) + sizeof(ActionList))
        + actionsSize;
}
#endif
    
#if CONTENT_EXTENSIONS_STATE_MACHINE_DEBUGGING
static String prefixTreeVertexToString(const PrefixTreeVertex& vertex, const HashMap<const PrefixTreeVertex*, ActionList>& actions, unsigned depth)
{
    StringBuilder builder;
    while (depth--)
        builder.appendLiteral("  ");
    builder.appendLiteral("vertex actions: ");

    auto actionsSlot = actions.find(&vertex);
    if (actionsSlot != actions.end()) {
        for (uint64_t action : actionsSlot->value) {
            builder.appendNumber(action);
            builder.append(',');
        }
    }
    builder.append('\n');
    return builder.toString();
}

static void recursivePrint(const PrefixTreeVertex& vertex, const HashMap<const PrefixTreeVertex*, ActionList>& actions, unsigned depth)
{
    dataLogF("%s", prefixTreeVertexToString(vertex, actions, depth).utf8().data());
    for (const auto& edge : vertex.edges) {
        StringBuilder builder;
        for (unsigned i = 0; i < depth * 2; ++i)
            builder.append(' ');
        builder.appendLiteral("vertex edge: ");
        builder.append(edge.term->toString());
        builder.append('\n');
        dataLogF("%s", builder.toString().utf8().data());
        ASSERT(edge.child);
        recursivePrint(*edge.child.get(), actions, depth + 1);
    }
}

void CombinedURLFilters::print() const
{
    recursivePrint(*m_prefixTreeRoot.get(), m_actions, 0);
}
#endif

CombinedURLFilters::CombinedURLFilters()
    : m_prefixTreeRoot(std::make_unique<PrefixTreeVertex>())
{
}

CombinedURLFilters::~CombinedURLFilters() = default;

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
            if (*lastPrefixTree->edges[i].term == term) {
                nextEntryIndex = i;
                break;
            }
        }
        if (nextEntryIndex != WTF::notFound)
            lastPrefixTree = lastPrefixTree->edges[nextEntryIndex].child.get();
        else {
            lastPrefixTree->edges.append(PrefixTreeEdge({m_alphabet.interned(term), std::make_unique<PrefixTreeVertex>()}));
            lastPrefixTree = lastPrefixTree->edges.last().child.get();
        }
    }

    auto addResult = m_actions.add(lastPrefixTree, ActionList());
    ActionList& actions = addResult.iterator->value;
    if (actions.find(actionId) == WTF::notFound)
        actions.append(actionId);
}

struct ActiveSubtree {
    ActiveSubtree(PrefixTreeVertex& vertex, ImmutableCharNFANodeBuilder&& nfaNode, unsigned edgeIndex)
        : vertex(vertex)
        , nfaNode(WTFMove(nfaNode))
        , edgeIndex(edgeIndex)
    {
    }
    PrefixTreeVertex& vertex;
    ImmutableCharNFANodeBuilder nfaNode;
    unsigned edgeIndex;
};

static void generateInfixUnsuitableForReverseSuffixTree(NFA& nfa, Vector<ActiveSubtree>& stack, const HashMap<const PrefixTreeVertex*, ActionList>& actions)
{
    // To avoid conflicts, we use the reverse suffix tree for subtrees that do not merge
    // in the prefix tree.
    //
    // We only unify the suffixes to the actions on the leaf.
    // If there are actions inside the tree, we generate the part of the subtree up to the action.
    //
    // If we accidentally insert a node with action inside the reverse-suffix-tree, we would create
    // new actions on unrelated pattern when unifying their suffixes.
    for (unsigned i = stack.size() - 1; i--;) {
        ActiveSubtree& activeSubtree = stack[i];
        if (activeSubtree.nfaNode.isValid())
            return;

        RELEASE_ASSERT_WITH_MESSAGE(i > 0, "The bottom of the stack must be the root of our fixed-length subtree. It should have it the isValid() case above.");

        auto actionsIterator = actions.find(&activeSubtree.vertex);
        bool hasActionInsideTree = actionsIterator != actions.end();

        // Stricto sensu, we should count the number of exit edges with fixed length.
        // That is costly and unlikely to matter in practice.
        bool hasSingleOutcome = activeSubtree.vertex.edges.size() == 1;

        if (hasActionInsideTree || !hasSingleOutcome) {
            // Go back to the end of the subtree that has already been generated.
            // From there, generate everything up to the vertex we found.
            unsigned end = i;
            unsigned beginning = end;

            ActiveSubtree* sourceActiveSubtree = nullptr;
            while (beginning--) {
                ActiveSubtree& activeSubtree = stack[beginning];
                if (activeSubtree.nfaNode.isValid()) {
                    sourceActiveSubtree = &activeSubtree;
                    break;
                }
            }
            ASSERT_WITH_MESSAGE(sourceActiveSubtree, "The root should always have a valid generator.");

            for (unsigned stackIndex = beginning + 1; stackIndex <= end; ++stackIndex) {
                ImmutableCharNFANodeBuilder& sourceNode = sourceActiveSubtree->nfaNode;
                ASSERT(sourceNode.isValid());
                auto& edge = sourceActiveSubtree->vertex.edges[sourceActiveSubtree->edgeIndex];

                ActiveSubtree& destinationActiveSubtree = stack[stackIndex];
                destinationActiveSubtree.nfaNode = edge.term->generateGraph(nfa, sourceNode, actions.get(&destinationActiveSubtree.vertex));

                sourceActiveSubtree = &destinationActiveSubtree;
            }

            return;
        }
    }
}

static void generateSuffixWithReverseSuffixTree(NFA& nfa, Vector<ActiveSubtree>& stack, const HashMap<const PrefixTreeVertex*, ActionList>& actions, ReverseSuffixTreeRoots& reverseSuffixTreeRoots)
{
    ActiveSubtree& leafSubtree = stack.last();
    ASSERT_WITH_MESSAGE(!leafSubtree.nfaNode.isValid(), "The leaf should never be generated by the code above, it should always be inserted into the prefix tree.");

    ActionList actionList = actions.get(&leafSubtree.vertex);
    ASSERT_WITH_MESSAGE(!actionList.isEmpty(), "Our prefix tree should always have actions on the leaves by construction.");

    HashableActionList hashableActionList(actionList);
    auto rootAddResult = reverseSuffixTreeRoots.add(hashableActionList, ReverseSuffixTreeVertex());
    if (rootAddResult.isNewEntry) {
        ImmutableCharNFANodeBuilder newNode(nfa);
        newNode.setActions(actionList.begin(), actionList.end());
        rootAddResult.iterator->value.nodeId = newNode.nodeId();
    }

    ReverseSuffixTreeVertex* activeReverseSuffixTreeVertex = &rootAddResult.iterator->value;
    uint32_t destinationNodeId = rootAddResult.iterator->value.nodeId;

    unsigned stackPosition = stack.size() - 2;
    while (true) {
        ActiveSubtree& source = stack[stackPosition];
        auto& edge = source.vertex.edges[source.edgeIndex];

        // This is the end condition: when we meet a node that has already been generated,
        // we just need to connect our backward tree to the forward tree.
        //
        // We *must not* add this last node to the reverse-suffix tree. That node can have
        // transitions back to earlier part of the prefix tree. If the prefix tree "caches"
        // such node, it would create new transitions that did not exist in the source language.
        if (source.nfaNode.isValid()) {
            stack.shrink(stackPosition + 1);
            edge.term->generateGraph(nfa, source.nfaNode, destinationNodeId);
            return;
        }
        --stackPosition;

        ASSERT_WITH_MESSAGE(!actions.contains(&source.vertex), "Any node with final actions should have been created before hitting the reverse suffix-tree.");

        ReverseSuffixTreeEdge* existingEdge = nullptr;
        for (ReverseSuffixTreeEdge& potentialExistingEdge : activeReverseSuffixTreeVertex->edges) {
            if (edge.term == potentialExistingEdge.term) {
                existingEdge = &potentialExistingEdge;
                break;
            }
        }

        if (existingEdge)
            activeReverseSuffixTreeVertex = existingEdge->child.get();
        else {
            ImmutableCharNFANodeBuilder newNode(nfa);
            edge.term->generateGraph(nfa, newNode, destinationNodeId);
            std::unique_ptr<ReverseSuffixTreeVertex> newVertex(new ReverseSuffixTreeVertex());
            newVertex->nodeId = newNode.nodeId();

            ReverseSuffixTreeVertex* newVertexAddress = newVertex.get();
            activeReverseSuffixTreeVertex->edges.append(ReverseSuffixTreeEdge({ edge.term, WTFMove(newVertex) }));
            activeReverseSuffixTreeVertex = newVertexAddress;
        }
        destinationNodeId = activeReverseSuffixTreeVertex->nodeId;

        ASSERT(source.vertex.edges.size() == 1);
        source.vertex.edges.clear();
    }

    RELEASE_ASSERT_NOT_REACHED();
}

static void clearReverseSuffixTree(ReverseSuffixTreeRoots& reverseSuffixTreeRoots)
{
    // We cannot rely on the destructor being called in order from top to bottom as we may overflow
    // the stack. Instead, we go depth first in the reverse-suffix-tree.

    for (auto& slot : reverseSuffixTreeRoots) {
        Vector<ReverseSuffixTreeVertex*, 128> stack;
        stack.append(&slot.value);

        while (true) {
            ReverseSuffixTreeVertex* top = stack.last();
            if (top->edges.isEmpty()) {
                stack.removeLast();
                if (stack.isEmpty())
                    break;
                stack.last()->edges.removeLast();
            } else
                stack.append(top->edges.last().child.get());
        }
    }
    reverseSuffixTreeRoots.clear();
}

static void generateNFAForSubtree(NFA& nfa, ImmutableCharNFANodeBuilder&& subtreeRoot, PrefixTreeVertex& root, const HashMap<const PrefixTreeVertex*, ActionList>& actions, size_t maxNFASize)
{
    // This recurses the subtree of the prefix tree.
    // For each edge that has fixed length (no quantifiers like ?, *, or +) it generates the nfa graph,
    // recurses into children, and deletes any processed leaf nodes.

    ReverseSuffixTreeRoots reverseSuffixTreeRoots;
    Vector<ActiveSubtree> stack;
    if (!root.edges.isEmpty())
        stack.append(ActiveSubtree(root, WTFMove(subtreeRoot), 0));

    bool nfaTooBig = false;
    
    // Generate graphs for each subtree that does not contain any quantifiers.
    while (!stack.isEmpty()) {
        PrefixTreeVertex& vertex = stack.last().vertex;
        const unsigned edgeIndex = stack.last().edgeIndex;

        if (edgeIndex < vertex.edges.size()) {
            auto& edge = vertex.edges[edgeIndex];

            // Clean up any processed leaves and return early if we are past the maxNFASize.
            if (nfaTooBig) {
                stack.last().edgeIndex = stack.last().vertex.edges.size();
                continue;
            }
            
            // Quantified edges in the subtree will be a part of another NFA.
            if (!edge.term->hasFixedLength()) {
                stack.last().edgeIndex++;
                continue;
            }

            ASSERT(edge.child.get());
            ImmutableCharNFANodeBuilder emptyBuilder;
            stack.append(ActiveSubtree(*edge.child.get(), WTFMove(emptyBuilder), 0));
        } else {
            bool isLeaf = vertex.edges.isEmpty();

            ASSERT(edgeIndex == vertex.edges.size());
            vertex.edges.removeAllMatching([](PrefixTreeEdge& edge)
            {
                return !edge.term;
            });

            if (isLeaf) {
                generateInfixUnsuitableForReverseSuffixTree(nfa, stack, actions);
                generateSuffixWithReverseSuffixTree(nfa, stack, actions, reverseSuffixTreeRoots);

                // Only stop generating an NFA at a leaf to ensure we have a correct NFA. We could go slightly over the maxNFASize.
                if (nfa.nodes.size() > maxNFASize)
                    nfaTooBig = true;
            } else
                stack.removeLast();

            if (!stack.isEmpty()) {
                auto& activeSubtree = stack.last();
                auto& edge = activeSubtree.vertex.edges[stack.last().edgeIndex];
                if (edge.child->edges.isEmpty())
                    edge.term = nullptr; // Mark this leaf for deleting.
                activeSubtree.edgeIndex++;
            }
        }
    }
    clearReverseSuffixTree(reverseSuffixTreeRoots);
}

void CombinedURLFilters::processNFAs(size_t maxNFASize, const WTF::Function<void(NFA&&)>& handler)
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
            if (!stack[stack.size() - 2]->edges.last().term->hasFixedLength())
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
                ImmutableCharNFANodeBuilder newNode = edge.term->generateGraph(nfa, lastNode, m_actions.get(edge.child.get()));
                lastNode = WTFMove(newNode);
            }

            // Put the non-quantified vertices in the subtree into the NFA and delete them.
            ASSERT(stack.last());
            generateNFAForSubtree(nfa, WTFMove(lastNode), *stack.last(), m_actions, maxNFASize);
        }
        nfa.finalize();

        handler(WTFMove(nfa));

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
