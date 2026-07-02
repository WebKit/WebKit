/*
 * Copyright (C) 2011-2017 Apple Inc. All rights reserved.
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

#pragma once

#include <ranges>
#include <wtf/CommaPrinter.h>
#include <wtf/DataLog.h>
#include <wtf/FastBitVector.h>
#include <wtf/GraphNodeWorklist.h>
#include <wtf/Vector.h>

namespace WTF {

// This is a utility for finding the dominators of a graph. Dominators are almost universally used
// for control flow graph analysis, so this code will refer to the graph's "nodes" as "blocks". In
// that regard this code is kind of specialized for the various JSC compilers, but you could use it
// for non-compiler things if you are OK with referring to your "nodes" as "blocks".

template<typename Graph>
class Dominators {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(Dominators);
public:
    using List = typename Graph::List;

    // Above this many nodes we switch from Semi-NCA to Lengauer-Tarjan. Semi-NCA has better
    // constant factors on real CFGs, but uses 16-bit DFS numbering, so it is also bounded by that.
    constexpr static unsigned maxNodesForSemiNCADominance = 20000;

    Dominators(Graph& graph, bool selfCheck = false)
        : m_graph(graph)
        , m_data(graph.template newMap<BlockData>())
    {
        if (m_graph.numNodes() <= maxNodesForSemiNCADominance) [[likely]] {
            SemiNCA semiNCA(m_graph);
            semiNCA.compute();

            for (unsigned blockIndex = m_graph.numNodes(); blockIndex--;) {
                typename Graph::Node block = m_graph.node(blockIndex);
                if (!block)
                    continue;

                typename Graph::Node idomBlock = semiNCA.immediateDominator(block);
                m_data[block].idomParent = idomBlock;
                if (idomBlock)
                    m_data[idomBlock].idomKids.append(block);
            }
        } else {
            LengauerTarjan lengauerTarjan(m_graph);
            lengauerTarjan.compute();

            for (unsigned blockIndex = m_graph.numNodes(); blockIndex--;) {
                typename Graph::Node block = m_graph.node(blockIndex);
                if (!block)
                    continue;

                typename Graph::Node idomBlock = lengauerTarjan.immediateDominator(block);
                m_data[block].idomParent = idomBlock;
                if (idomBlock)
                    m_data[idomBlock].idomKids.append(block);
            }
        }

        // From here we want to build a spanning tree with both upward and downward links and we want
        // to do a search over this tree to compute pre and post numbers that can be used for dominance
        // tests.
    
        unsigned nextPreNumber = 0;
        unsigned nextPostNumber = 0;
    
        // Plain stack-based worklist because we are guaranteed to see each block exactly once anyway.
        Vector<GraphNodeWithOrder<typename Graph::Node>> worklist;
        worklist.append(GraphNodeWithOrder<typename Graph::Node>(m_graph.root(), GraphVisitOrder::Pre));
        while (!worklist.isEmpty()) {
            GraphNodeWithOrder<typename Graph::Node> item = worklist.takeLast();
            switch (item.order) {
            case GraphVisitOrder::Pre:
                m_data[item.node].preNumber = nextPreNumber++;
                worklist.append(GraphNodeWithOrder<typename Graph::Node>(item.node, GraphVisitOrder::Post));
                for (typename Graph::Node kid : m_data[item.node].idomKids)
                    worklist.append(GraphNodeWithOrder<typename Graph::Node>(kid, GraphVisitOrder::Pre));
                break;
            case GraphVisitOrder::Post:
                m_data[item.node].postNumber = nextPostNumber++;
                break;
            }
        }
    
        if (selfCheck) {
            // Check our dominator calculation:
            // 1) Check that our range-based ancestry test is the same as a naive ancestry test.
            // 2) Check that our notion of who dominates whom is identical to a naive (not
            //    Lengauer-Tarjan) dominator calculation.
        
            ValidationContext context(m_graph, *this);
        
            for (unsigned fromBlockIndex = m_graph.numNodes(); fromBlockIndex--;) {
                typename Graph::Node fromBlock = m_graph.node(fromBlockIndex);
                if (!fromBlock || m_data[fromBlock].preNumber == UINT_MAX)
                    continue;
                for (unsigned toBlockIndex = m_graph.numNodes(); toBlockIndex--;) {
                    typename Graph::Node toBlock = m_graph.node(toBlockIndex);
                    if (!toBlock || m_data[toBlock].preNumber == UINT_MAX)
                        continue;
                
                    if (dominates(fromBlock, toBlock) != naiveDominates(fromBlock, toBlock))
                        context.reportError(fromBlock, toBlock, "Range-based domination check is broken"_s);
                    if (dominates(fromBlock, toBlock) != context.naiveDominators.dominates(fromBlock, toBlock))
                        context.reportError(fromBlock, toBlock, "Lengauer-Tarjan domination is broken"_s);
                }
            }
        
            context.handleErrors();
        }
    }

    bool strictlyDominates(typename Graph::Node from, typename Graph::Node to) const
    {
        return m_data[to].preNumber > m_data[from].preNumber
            && m_data[to].postNumber < m_data[from].postNumber;
    }
    
    bool dominates(typename Graph::Node from, typename Graph::Node to) const
    {
        return from == to || strictlyDominates(from, to);
    }

    // Returns the immediate dominator of this block. Returns null for the root block.
    typename Graph::Node idom(typename Graph::Node block) const
    {
        return m_data[block].idomParent;
    }
    
    template<typename Functor>
    void forAllStrictDominatorsOf(typename Graph::Node to, const Functor& functor) const
    {
        for (typename Graph::Node block = m_data[to].idomParent; block; block = m_data[block].idomParent)
            functor(block);
    }
    
    // Note: This will visit the dominators starting with the 'to' node and moving up the idom tree
    // until it gets to the root. Some clients of this function, like B3::moveConstants(), rely on this
    // order.
    template<typename Functor>
    void forAllDominatorsOf(typename Graph::Node to, const Functor& functor) const
    {
        for (typename Graph::Node block = to; block; block = m_data[block].idomParent)
            functor(block);
    }
    
    template<typename Functor>
    void forAllBlocksStrictlyDominatedBy(typename Graph::Node from, const Functor& functor) const
    {
        Vector<typename Graph::Node, 16> worklist;
        worklist.appendVector(m_data[from].idomKids);
        while (!worklist.isEmpty()) {
            typename Graph::Node block = worklist.takeLast();
            functor(block);
            worklist.appendVector(m_data[block].idomKids);
        }
    }
    
    template<typename Functor>
    void forAllBlocksDominatedBy(typename Graph::Node from, const Functor& functor) const
    {
        Vector<typename Graph::Node, 16> worklist;
        worklist.append(from);
        while (!worklist.isEmpty()) {
            typename Graph::Node block = worklist.takeLast();
            functor(block);
            worklist.appendVector(m_data[block].idomKids);
        }
    }
    
    typename Graph::Set strictDominatorsOf(typename Graph::Node to) const
    {
        typename Graph::Set result;
        forAllStrictDominatorsOf(
            to,
            [&] (typename Graph::Node node) {
                result.add(node);
            });
        return result;
    }
    
    typename Graph::Set dominatorsOf(typename Graph::Node to) const
    {
        typename Graph::Set result;
        forAllDominatorsOf(
            to,
            [&] (typename Graph::Node node) {
                result.add(node);
            });
        return result;
    }
    
    typename Graph::Set blocksStrictlyDominatedBy(typename Graph::Node from) const
    {
        typename Graph::Set result;
        forAllBlocksStrictlyDominatedBy(
            from,
            [&] (typename Graph::Node node) {
                result.add(node);
            });
        return result;
    }
    
    typename Graph::Set blocksDominatedBy(typename Graph::Node from) const
    {
        typename Graph::Set result;
        forAllBlocksDominatedBy(
            from,
            [&] (typename Graph::Node node) {
                result.add(node);
            });
        return result;
    }
    
    template<typename Functor>
    void forAllBlocksInDominanceFrontierOf(
        typename Graph::Node from, const Functor& functor) const
    {
        typename Graph::Set set;
        forAllBlocksInDominanceFrontierOfImpl(
            from,
            [&] (typename Graph::Node block) {
                if (set.add(block))
                    functor(block);
            });
    }
    
    typename Graph::Set dominanceFrontierOf(typename Graph::Node from) const
    {
        typename Graph::Set result;
        forAllBlocksInDominanceFrontierOf(
            from,
            [&] (typename Graph::Node node) {
                result.add(node);
            });
        return result;
    }
    
    template<typename Functor>
    void forAllBlocksInIteratedDominanceFrontierOf(const List& from, const Functor& functor)
    {
        forAllBlocksInPrunedIteratedDominanceFrontierOf(
            from,
            [&] (typename Graph::Node block) -> bool {
                functor(block);
                return true;
            });
    }
    
    // This is a close relative of forAllBlocksInIteratedDominanceFrontierOf(), which allows the
    // given functor to return false to indicate that we don't wish to consider the given block.
    // Useful for computing pruned SSA form.
    template<typename Functor>
    void forAllBlocksInPrunedIteratedDominanceFrontierOf(
        const List& from, const Functor& functor)
    {
        typename Graph::Set set;
        forAllBlocksInIteratedDominanceFrontierOfImpl(
            from,
            [&] (typename Graph::Node block) -> bool {
                if (!set.add(block))
                    return false;
                return functor(block);
            });
    }
    
    typename Graph::Set iteratedDominanceFrontierOf(const List& from) const
    {
        typename Graph::Set result;
        forAllBlocksInIteratedDominanceFrontierOfImpl(
            from,
            [&] (typename Graph::Node node) -> bool {
                return result.add(node);
            });
        return result;
    }
    
    void dump(PrintStream& out) const
    {
        for (unsigned blockIndex = 0; blockIndex < m_data.size(); ++blockIndex) {
            if (m_data[blockIndex].preNumber == UINT_MAX)
                continue;
            
            out.print("    Block #"_s, blockIndex, ": idom = "_s, m_graph.dump(m_data[blockIndex].idomParent), ", idomKids = ["_s);
            CommaPrinter comma;
            for (unsigned i = 0; i < m_data[blockIndex].idomKids.size(); ++i)
                out.print(comma, m_graph.dump(m_data[blockIndex].idomKids[i]));
            out.print("], pre/post = "_s, m_data[blockIndex].preNumber, "/"_s, m_data[blockIndex].postNumber, "\n"_s);
        }
    }
    
private:
    // This implements the Semi-NCA dominator algorithm, the same approach used by LLVM.
    // Like Lengauer-Tarjan it computes semidominators using a depth-first spanning tree and
    // a path-compressing EVAL, but it replaces Lengauer-Tarjan's bucket/LINK machinery for
    // deriving immediate dominators with a simple "nearest common ancestor" walk over the
    // partially-built dominator tree. This is near-linear in practice, and due to simplicity,
    // this is in general faster than Lengauer-Tarjan. However since this is *near-linear*,
    // pathological graph can explode the computation. Thus for large graph, we fall back to
    // Lengauer-Tarjan to avoid too-long computation time.
    //
    // Semi-NCA is described in "Linear-Time Algorithms for Dominators and Related Problems", 2005, Georgiadis, Loukas.
    // https://www.cs.princeton.edu/research/techreps/432

    class SemiNCA {
        WTF_DEPRECATED_MAKE_FAST_ALLOCATED(SemiNCA);
        // 0 is reserved as "no DFS number" / "no ancestor", so DFS numbers are 1-based.
        constexpr static uint16_t noDFSNumber = 0;
    public:
        SemiNCA(Graph& graph)
            : m_graph(graph)
        {
            // We only use this for small-ish graphs (Lengauer-Tarjan handles the rest), so we can use
            // 16-bit DFS numbers. With 1-based numbering we need numNodes() + 1 to be representable.
            RELEASE_ASSERT(graph.numNodes() < std::numeric_limits<uint16_t>::max());
        }

        void compute()
        {
            unsigned numNodes = m_graph.numNodes();
            // node index -> DFS number, initialized to noDFSNumber (0) for every node.
            m_nodeToDFS.fill(noDFSNumber, numNodes);
            // All per-DFS-number state lives in one array for locality. noDFSNumber is 0, so a
            // zero-initialized BlockData is the correct starting state for every field.
            m_data.fill(BlockData { }, numNodes + 1);

            // Step 1: assign DFS numbers and record each node's parent in the DFS spanning tree.
            uint16_t count = 0;
            {
                struct WorklistItem {
                    uint16_t nodeIndex;
                    uint16_t parentDFS;
                };
                uint16_t nextDFSNumber = 0;
                Vector<WorklistItem, 64> worklist;
                worklist.append({ static_cast<uint16_t>(m_graph.index(m_graph.root())), noDFSNumber });
                while (!worklist.isEmpty()) {
                    auto item = worklist.takeLast();
                    if (m_nodeToDFS[item.nodeIndex] != noDFSNumber)
                        continue;
                    uint16_t dfs = ++nextDFSNumber;
                    m_nodeToDFS[item.nodeIndex] = dfs;
                    BlockData& data = m_data[dfs];
                    data.nodeIndex = item.nodeIndex;
                    data.parent = item.parentDFS;
                    data.semi = dfs;
                    data.label = dfs;
                    for (auto successor : m_graph.successors(m_graph.node(item.nodeIndex))) {
                        uint16_t successorIndex = m_graph.index(successor);
                        if (m_nodeToDFS[successorIndex] == noDFSNumber)
                            worklist.append({ successorIndex, dfs });
                    }
                }
                count = nextDFSNumber;
            }

            // Step 2: compute semidominators in reverse DFS order, then LINK each node to its parent.
            // Semidominator means, let's avoid the normal dfs path, and what the earliest (small DFS num)
            // point can start reaching to the node without using the normal dfs path, like a shortcut.
            //
            // The intent of Semi-NCA algorithm is, idom(w) = NearestCommonAncestor(parent(w), semi(w)).
            {
                // Path-compressing EVAL: returns the node on the ancestor path from v with minimum
                // semidominator. We compress the path iteratively to avoid deep recursion.
                Vector<uint16_t, 64> compressStack;
                auto eval = [&](uint16_t v) -> uint16_t {
                    if (m_data[v].ancestor == noDFSNumber)
                        return m_data[v].label;

                    compressStack.shrink(0);
                    uint16_t u = v;
                    while (m_data[m_data[u].ancestor].ancestor != noDFSNumber) {
                        compressStack.append(u);
                        u = m_data[u].ancestor;
                    }
                    for (unsigned i = compressStack.size(); i--;) {
                        BlockData& data = m_data[compressStack[i]];
                        uint16_t ancestor = data.ancestor;
                        if (m_data[m_data[ancestor].label].semi < m_data[data.label].semi)
                            data.label = m_data[ancestor].label;
                        data.ancestor = m_data[ancestor].ancestor;
                    }
                    return m_data[v].label;
                };

                for (uint16_t w = count; w >= 2; --w) {
                    BlockData& wData = m_data[w];
                    for (auto predecessor : m_graph.predecessors(m_graph.node(wData.nodeIndex))) {
                        uint16_t v = m_nodeToDFS[m_graph.index(predecessor)];
                        if (v == noDFSNumber)
                            continue; // Predecessor not reachable from the root.
                        uint16_t u = eval(v);
                        wData.semi = std::min(wData.semi, m_data[u].semi);
                    }
                    wData.ancestor = wData.parent;
                }
            }

            // Step 3: derive immediate dominators. Start each node's idom at its DFS parent and walk
            // it up the (already-computed) dominator tree until it is an ancestor of the node's
            // semidominator. This is the "nearest common ancestor" refinement.
            //
            // We do not need to walk up in semidominator side. This is because semidominator is guaranteed
            // to be on the path between idom(w) -> ... -> parent(w).
            if (count >= 1)
                m_data[1].idom = 1;
            for (uint16_t w = 2; w <= count; ++w) {
                uint16_t idom = m_data[w].parent;
                while (idom > m_data[w].semi)
                    idom = m_data[idom].idom;
                m_data[w].idom = idom;
            }
        }

        typename Graph::Node immediateDominator(typename Graph::Node block)
        {
            uint16_t dfs = m_nodeToDFS[m_graph.index(block)];
            // The root (DFS number 1) and any node unreachable from the root have no idom.
            if (dfs <= 1)
                return nullptr;
            return m_graph.node(m_data[m_data[dfs].idom].nodeIndex);
        }

    private:
        // All numeric fields are indexed by DFS number. noDFSNumber (0) is the value-initialized
        // default. We keep nodeIndex as a uint16_t (not a Graph::Node pointer) so BlockData stays
        // compact at 12 bytes; the node is reconstructed via m_graph.node() at the few boundaries.
        struct BlockData {
            uint16_t nodeIndex { 0 };  // DFS number -> node index
            uint16_t parent { 0 };     // DFS-tree parent's DFS number
            uint16_t semi { 0 };       // semidominator (DFS number)
            uint16_t label { 0 };      // EVAL label
            uint16_t ancestor { 0 };   // forest ancestor (noDFSNumber if none)
            uint16_t idom { 0 };       // immediate dominator (DFS number)
        };

        Graph& m_graph;
        Vector<uint16_t, 64> m_nodeToDFS;  // node index -> DFS number (noDFSNumber if unreachable)
        Vector<BlockData, 64> m_data;      // DFS number -> per-node Semi-NCA state
    };

    // This implements Lengauer and Tarjan's "A Fast Algorithm for Finding Dominators in a Flowgraph"
    // (TOPLAS 1979). It uses the "simple" implementation of LINK and EVAL, which yields an O(n log n)
    // solution. The full paper is linked below; this code attempts to closely follow the algorithm as
    // it is presented in the paper; in particular sections 3 and 4 as well as appendix B.
    // https://www.cs.princeton.edu/courses/archive/fall03/cs528/handouts/a%20fast%20algorithm%20for%20finding.pdf
    //
    // This code is very subtle. The Lengauer-Tarjan algorithm is incredibly deep to begin with. The
    // goal of this code is to follow the code in the paper, however our implementation must deviate
    // from the paper when it comes to recursion. The authors had used recursion to implement DFS, and
    // also to implement the "simple" EVAL. We convert both of those into worklist-based solutions.
    // Finally, once the algorithm gives us immediate dominators, we implement dominance tests by
    // walking the dominator tree and computing pre and post numbers. We then use the range inclusion
    // check trick that was first discovered by Paul F. Dietz in 1982 in "Maintaining order in a linked
    // list" (see http://dl.acm.org/citation.cfm?id=802184).

    class LengauerTarjan {
        WTF_DEPRECATED_MAKE_FAST_ALLOCATED(LengauerTarjan);
    public:
        LengauerTarjan(Graph& graph)
            : m_graph(graph)
            , m_data(graph.template newMap<BlockData>())
        {
            for (unsigned blockIndex = m_graph.numNodes(); blockIndex--;) {
                typename Graph::Node block = m_graph.node(blockIndex);
                if (!block)
                    continue;
                m_data[block].label = block;
            }
        }
    
        void compute()
        {
            computeDepthFirstPreNumbering(); // Step 1.
            computeSemiDominatorsAndImplicitImmediateDominators(); // Steps 2 and 3.
            computeExplicitImmediateDominators(); // Step 4.
        }
    
        typename Graph::Node immediateDominator(typename Graph::Node block)
        {
            return m_data[block].dom;
        }

    private:
        void computeDepthFirstPreNumbering()
        {
            // Use a block worklist that also tracks the index inside the successor list. This is
            // necessary for ensuring that we don't attempt to visit a successor until the previous
            // successors that we had visited are fully processed. This ends up being revealed in the
            // output of this method because the first time we see an edge to a block, we set the
            // block's parent. So, if we have:
            //
            // A -> B
            // A -> C
            // B -> C
            //
            // And we're processing A, then we want to ensure that if we see A->B first (and hence set
            // B's prenumber before we set C's) then we also end up setting C's parent to B by virtue
            // of not noticing A->C until we're done processing B.

            ExtendedGraphNodeWorklist<typename Graph::Node, unsigned, typename Graph::Set> worklist;
            worklist.push(m_graph.root(), 0);
        
            while (GraphNodeWith<typename Graph::Node, unsigned> item = worklist.pop()) {
                typename Graph::Node block = item.node;
                unsigned successorIndex = item.data;
            
                // We initially push with successorIndex = 0 regardless of whether or not we have any
                // successors. This is so that we can assign our prenumber. Subsequently we get pushed
                // with higher successorIndex values, but only if they are in range.
                ASSERT(!successorIndex || successorIndex < m_graph.successors(block).size());

                if (!successorIndex) {
                    m_data[block].semiNumber = m_blockByPreNumber.size();
                    m_blockByPreNumber.append(block);
                }
            
                if (successorIndex < m_graph.successors(block).size()) {
                    unsigned nextSuccessorIndex = successorIndex + 1;
                    if (nextSuccessorIndex < m_graph.successors(block).size())
                        worklist.forcePush(block, nextSuccessorIndex);

                    typename Graph::Node successorBlock = m_graph.successors(block)[successorIndex];
                    if (worklist.push(successorBlock, 0))
                        m_data[successorBlock].parent = block;
                }
            }
        }
    
        void computeSemiDominatorsAndImplicitImmediateDominators()
        {
            for (unsigned currentPreNumber = m_blockByPreNumber.size(); currentPreNumber-- > 1;) {
                typename Graph::Node block = m_blockByPreNumber[currentPreNumber];
                BlockData& blockData = m_data[block];
            
                // Step 2:
                for (typename Graph::Node predecessorBlock : m_graph.predecessors(block)) {
                    typename Graph::Node intermediateBlock = eval(predecessorBlock);
                    blockData.semiNumber = std::min(
                        m_data[intermediateBlock].semiNumber, blockData.semiNumber);
                }
                unsigned bucketPreNumber = blockData.semiNumber;
                ASSERT(bucketPreNumber <= currentPreNumber);
                m_data[m_blockByPreNumber[bucketPreNumber]].bucket.append(block);
                link(blockData.parent, block);
            
                // Step 3:
                for (typename Graph::Node semiDominee : m_data[blockData.parent].bucket) {
                    typename Graph::Node possibleDominator = eval(semiDominee);
                    BlockData& semiDomineeData = m_data[semiDominee];
                    ASSERT(m_blockByPreNumber[semiDomineeData.semiNumber] == blockData.parent);
                    BlockData& possibleDominatorData = m_data[possibleDominator];
                    if (possibleDominatorData.semiNumber < semiDomineeData.semiNumber)
                        semiDomineeData.dom = possibleDominator;
                    else
                        semiDomineeData.dom = blockData.parent;
                }
                m_data[blockData.parent].bucket.clear();
            }
        }
    
        void computeExplicitImmediateDominators()
        {
            for (unsigned currentPreNumber = 1; currentPreNumber < m_blockByPreNumber.size(); ++currentPreNumber) {
                typename Graph::Node block = m_blockByPreNumber[currentPreNumber];
                BlockData& blockData = m_data[block];
            
                if (blockData.dom != m_blockByPreNumber[blockData.semiNumber])
                    blockData.dom = m_data[blockData.dom].dom;
            }
        }
    
        void link(typename Graph::Node from, typename Graph::Node to)
        {
            m_data[to].ancestor = from;
        }
    
        typename Graph::Node eval(typename Graph::Node block)
        {
            if (!m_data[block].ancestor)
                return block;
        
            compress(block);
            return m_data[block].label;
        }
    
        void compress(typename Graph::Node initialBlock)
        {
            // This was meant to be a recursive function, but we don't like recursion because we don't
            // want to blow the stack. The original function will call compress() recursively on the
            // ancestor of anything that has an ancestor. So, we populate our worklist with the
            // recursive ancestors of initialBlock. Then we process the list starting from the block
            // that is furthest up the ancestor chain.
        
            typename Graph::Node ancestor = m_data[initialBlock].ancestor;
            ASSERT(ancestor);
            if (!m_data[ancestor].ancestor)
                return;
        
            Vector<typename Graph::Node, 16> stack;
            for (typename Graph::Node block = initialBlock; block; block = m_data[block].ancestor)
                stack.append(block);
        
            // We only care about blocks that have an ancestor that has an ancestor. The last two
            // elements in the stack won't satisfy this property.
            ASSERT(stack.size() >= 2);
            ASSERT(!m_data[stack[stack.size() - 1]].ancestor);
            ASSERT(!m_data[m_data[stack[stack.size() - 2]].ancestor].ancestor);
        
            for (unsigned i = stack.size() - 2; i--;) {
                typename Graph::Node block = stack[i];
                typename Graph::Node& labelOfBlock = m_data[block].label;
                typename Graph::Node& ancestorOfBlock = m_data[block].ancestor;
                ASSERT(ancestorOfBlock);
                ASSERT(m_data[ancestorOfBlock].ancestor);
            
                typename Graph::Node labelOfAncestorOfBlock = m_data[ancestorOfBlock].label;
            
                if (m_data[labelOfAncestorOfBlock].semiNumber < m_data[labelOfBlock].semiNumber)
                    labelOfBlock = labelOfAncestorOfBlock;
                ancestorOfBlock = m_data[ancestorOfBlock].ancestor;
            }
        }

        struct BlockData {
            WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED(BlockData);

            BlockData()
                : parent(nullptr)
                , preNumber(UINT_MAX)
                , semiNumber(UINT_MAX)
                , ancestor(nullptr)
                , label(nullptr)
                , dom(nullptr)
            {
            }
        
            typename Graph::Node parent;
            unsigned preNumber;
            unsigned semiNumber;
            typename Graph::Node ancestor;
            typename Graph::Node label;
            Vector<typename Graph::Node> bucket;
            typename Graph::Node dom;
        };
    
        Graph& m_graph;
        typename Graph::template Map<BlockData> m_data;
        Vector<typename Graph::Node> m_blockByPreNumber;
    };

    class NaiveDominators {
        WTF_DEPRECATED_MAKE_FAST_ALLOCATED(NaiveDominators);
    public:
        NaiveDominators(Graph& graph)
            : m_graph(graph)
        {
            // This implements a naive dominator solver.

            ASSERT(!graph.predecessors(graph.root()).size());
    
            unsigned numBlocks = graph.numNodes();
    
            // Allocate storage for the dense dominance matrix. 
            m_results.grow(numBlocks);
            for (unsigned i = numBlocks; i--;)
                m_results[i].grow(numBlocks);
            m_scratch.resize(numBlocks);

            // We know that the entry block is only dominated by itself.
            m_results[0].clearAll();
            m_results[0][0] = true;

            // Find all of the valid blocks.
            m_scratch.clearAll();
            for (unsigned i = numBlocks; i--;) {
                if (!graph.node(i))
                    continue;
                m_scratch[i] = true;
            }
    
            // Mark all nodes as dominated by everything.
            for (unsigned i = numBlocks; i-- > 1;) {
                if (!graph.node(i) || !graph.predecessors(graph.node(i)).size())
                    m_results[i].clearAll();
                else
                    m_results[i] = m_scratch;
            }

            // Iteratively eliminate nodes that are not dominator.
            bool changed;
            do {
                changed = false;
                // Prune dominators in all non entry blocks: forward scan.
                for (unsigned i = 1; i < numBlocks; ++i)
                    changed |= pruneDominators(i);

                if (!changed)
                    break;

                // Prune dominators in all non entry blocks: backward scan.
                changed = false;
                for (unsigned i = numBlocks; i-- > 1;)
                    changed |= pruneDominators(i);
            } while (changed);
        }
        
        bool dominates(unsigned from, unsigned to) const
        {
            return m_results[to][from];
        }
    
        bool dominates(typename Graph::Node from, typename Graph::Node to) const
        {
            return dominates(m_graph.index(from), m_graph.index(to));
        }
    
        void dump(PrintStream& out) const
        {
            for (unsigned blockIndex = 0; blockIndex < m_graph.numNodes(); ++blockIndex) {
                typename Graph::Node block = m_graph.node(blockIndex);
                if (!block)
                    continue;
                out.print("    Block ", m_graph.dump(block), ":");
                for (unsigned otherIndex = 0; otherIndex < m_graph.numNodes(); ++otherIndex) {
                    if (!dominates(m_graph.index(block), otherIndex))
                        continue;
                    out.print(" ", m_graph.dump(m_graph.node(otherIndex)));
                }
                out.print("\n");
            }
        }
    
    private:
        bool pruneDominators(unsigned idx)
        {
            typename Graph::Node block = m_graph.node(idx);

            if (!block || !m_graph.predecessors(block).size())
                return false;

            // Find the intersection of dom(preds).
            m_scratch = m_results[m_graph.index(m_graph.predecessors(block)[0])];
            for (unsigned j = m_graph.predecessors(block).size(); j-- > 1;)
                m_scratch &= m_results[m_graph.index(m_graph.predecessors(block)[j])];

            // The block is also dominated by itself.
            m_scratch[idx] = true;

            return m_results[idx].setAndCheck(m_scratch);
        }
    
        Graph& m_graph;
        Vector<FastBitVector> m_results; // For each block, the bitvector of blocks that dominate it.
        FastBitVector m_scratch; // A temporary bitvector with bit for each block. We recycle this to save new/deletes.
    };

    struct ValidationContext {
        WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED(ValidationContext);

        ValidationContext(Graph& graph, Dominators& dominators)
            : graph(graph)
            , dominators(dominators)
            , naiveDominators(graph)
        {
        }
    
        void reportError(typename Graph::Node from, typename Graph::Node to, ASCIILiteral message)
        {
            Error error;
            error.from = from;
            error.to = to;
            error.message = message;
            errors.append(error);
        }
    
        void handleErrors()
        {
            if (errors.isEmpty())
                return;
        
            dataLog("DFG DOMINATOR VALIDATION FAILED:\n");
            dataLog("\n");
            dataLog("For block domination relationships:\n");
            for (unsigned i = 0; i < errors.size(); ++i) {
                dataLog(
                    "    ", graph.dump(errors[i].from), " -> ", graph.dump(errors[i].to),
                    " (", errors[i].message, ")\n");
            }
            dataLog("\n");
            dataLog("Control flow graph:\n");
            for (unsigned blockIndex = 0; blockIndex < graph.numNodes(); ++blockIndex) {
                typename Graph::Node block = graph.node(blockIndex);
                if (!block)
                    continue;
                dataLog("    Block ", graph.dump(graph.node(blockIndex)), ": successors = [");
                CommaPrinter comma;
                for (auto successor : graph.successors(block))
                    dataLog(comma, graph.dump(successor));
                dataLog("], predecessors = [");
                comma = CommaPrinter();
                for (auto predecessor : graph.predecessors(block))
                    dataLog(comma, graph.dump(predecessor));
                dataLog("]\n");
            }
            dataLog("\n");
            dataLog("Lengauer-Tarjan Dominators:\n");
            dataLog(dominators);
            dataLog("\n");
            dataLog("Naive Dominators:\n");
            naiveDominators.dump(WTF::dataFile());
            dataLog("\n");
            dataLog("Graph at time of failure:\n");
            dataLog(graph);
            dataLog("\n");
            dataLog("DFG DOMINATOR VALIDATION FAILIED!\n");
            CRASH();
        }
    
        Graph& graph;
        Dominators& dominators;
        NaiveDominators naiveDominators;
    
        struct Error {
            WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED(Error);

            typename Graph::Node from;
            typename Graph::Node to;
            ASCIILiteral message;
        };
    
        Vector<Error> errors;
    };

    bool naiveDominates(typename Graph::Node from, typename Graph::Node to) const
    {
        for (typename Graph::Node block = to; block; block = m_data[block].idomParent) {
            if (block == from)
                return true;
        }
        return false;
    }
    
    template<typename Functor>
    void forAllBlocksInDominanceFrontierOfImpl(
        typename Graph::Node from, const Functor& functor) const
    {
        // Paraphrasing from http://en.wikipedia.org/wiki/Dominator_(graph_theory):
        //     "The dominance frontier of a block 'from' is the set of all blocks 'to' such that
        //     'from' dominates an immediate predecessor of 'to', but 'from' does not strictly
        //     dominate 'to'."
        //
        // A useful corner case to remember: a block may be in its own dominance frontier if it has
        // a loop edge to itself, since it dominates itself and so it dominates its own immediate
        // predecessor, and a block never strictly dominates itself.
        
        forAllBlocksDominatedBy(
            from,
            [&] (typename Graph::Node block) {
                for (typename Graph::Node to : m_graph.successors(block)) {
                    if (!strictlyDominates(from, to))
                        functor(to);
                }
            });
    }
    
    template<typename Functor>
    void forAllBlocksInIteratedDominanceFrontierOfImpl(
        const List& from, const Functor& functor) const
    {
        List worklist = from;
        while (!worklist.isEmpty()) {
            typename Graph::Node block = worklist.takeLast();
            forAllBlocksInDominanceFrontierOfImpl(
                block,
                [&] (typename Graph::Node otherBlock) {
                    if (functor(otherBlock))
                        worklist.append(otherBlock);
                });
        }
    }
    
    struct BlockData {
        WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED(BlockData);

        BlockData()
            : idomParent(nullptr)
            , preNumber(UINT_MAX)
            , postNumber(UINT_MAX)
        {
        }
        
        Vector<typename Graph::Node> idomKids;
        typename Graph::Node idomParent;
        
        unsigned preNumber;
        unsigned postNumber;
    };
    
    Graph& m_graph;
    typename Graph::template Map<BlockData> m_data;
};

} // namespace WTF

using WTF::Dominators;
