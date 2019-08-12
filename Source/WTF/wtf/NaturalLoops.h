/*
 * Copyright (C) 2013-2017 Apple Inc. All rights reserved.
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

#include <wtf/Dominators.h>

namespace WTF {

template<typename Graph>
class NaturalLoops;

template<typename Graph>
class NaturalLoop {
    WTF_MAKE_FAST_ALLOCATED;
public:
    NaturalLoop()
        : m_graph(nullptr)
        , m_header(nullptr)
        , m_outerLoopIndex(UINT_MAX)
    {
    }
    
    NaturalLoop(Graph& graph, typename Graph::Node header, unsigned index)
        : m_graph(&graph)
        , m_header(header)
        , m_outerLoopIndex(UINT_MAX)
        , m_index(index)
    {
    }
    
    Graph* graph() const { return m_graph; }
    
    typename Graph::Node header() const { return m_header; }
    
    unsigned size() const { return m_body.size(); }
    typename Graph::Node at(unsigned i) const { return m_body[i]; }
    typename Graph::Node operator[](unsigned i) const { return at(i); }

    // This is the slower, but simpler, way of asking if a block belongs to
    // a natural loop. It's faster to call NaturalLoops::belongsTo(), which
    // tries to be O(loop depth) rather than O(loop size). Loop depth is
    // almost always smaller than loop size. A *lot* smaller.
    bool contains(typename Graph::Node block) const
    {
        for (unsigned i = m_body.size(); i--;) {
            if (m_body[i] == block)
                return true;
        }
        ASSERT(block != header()); // Header should be contained.
        return false;
    }

    // The index of this loop in NaturalLoops.
    unsigned index() const { return m_index; }
    
    bool isOuterMostLoop() const { return m_outerLoopIndex == UINT_MAX; }
    
    void dump(PrintStream& out) const
    {
        if (!m_graph) {
            out.print("<null>");
            return;
        }
        
        out.print("[Header: ", m_graph->dump(header()), ", Body:");
        for (unsigned i = 0; i < m_body.size(); ++i)
            out.print(" ", m_graph->dump(m_body[i]));
        out.print("]");
    }
    
private:
    template<typename>
    friend class NaturalLoops;
    
    void addBlock(typename Graph::Node block)
    {
        ASSERT(!m_body.contains(block)); // The NaturalLoops algorithm relies on blocks being unique in this vector.
        m_body.append(block);
    }

    Graph* m_graph;
    typename Graph::Node m_header;
    Vector<typename Graph::Node, 4> m_body;
    unsigned m_outerLoopIndex;
    unsigned m_index;
};

template<typename Graph>
class NaturalLoops {
    WTF_MAKE_FAST_ALLOCATED;
public:
    typedef std::array<unsigned, 2> InnerMostLoopIndices;

    NaturalLoops(Graph& graph, Dominators<Graph>& dominators, bool selfCheck = false)
        : m_graph(graph)
        , m_innerMostLoopIndices(graph.template newMap<InnerMostLoopIndices>())
    {
        // Implement the classic dominator-based natural loop finder. The first
        // step is to find all control flow edges A -> B where B dominates A.
        // Then B is a loop header and A is a backward branching block. We will
        // then accumulate, for each loop header, multiple backward branching
        // blocks. Then we backwards graph search from the backward branching
        // blocks to their loop headers, which gives us all of the blocks in the
        // loop body.
    
        static const bool verbose = false;
    
        if (verbose) {
            dataLog("Dominators:\n");
            dominators.dump(WTF::dataFile());
        }
    
        m_loops.shrink(0);
    
        for (unsigned blockIndex = graph.numNodes(); blockIndex--;) {
            typename Graph::Node header = graph.node(blockIndex);
            if (!header)
                continue;
        
            for (unsigned i = graph.predecessors(header).size(); i--;) {
                typename Graph::Node footer = graph.predecessors(header)[i];
                if (!dominators.dominates(header, footer))
                    continue;
                // At this point, we've proven 'header' is actually a loop header and
                // that 'footer' is a loop footer.
                bool found = false;
                for (unsigned j = m_loops.size(); j--;) {
                    if (m_loops[j].header() == header) {
                        m_loops[j].addBlock(footer);
                        found = true;
                        break;
                    }
                }
                if (found)
                    continue;
                NaturalLoop<Graph> loop(graph, header, m_loops.size());
                loop.addBlock(footer);
                m_loops.append(loop);
            }
        }
    
        if (verbose)
            dataLog("After bootstrap: ", *this, "\n");
    
        FastBitVector seenBlocks;
        Vector<typename Graph::Node, 4> blockWorklist;
        seenBlocks.resize(graph.numNodes());
    
        for (unsigned i = m_loops.size(); i--;) {
            NaturalLoop<Graph>& loop = m_loops[i];
        
            seenBlocks.clearAll();
            ASSERT(blockWorklist.isEmpty());
        
            if (verbose)
                dataLog("Dealing with loop ", loop, "\n");
        
            for (unsigned j = loop.size(); j--;) {
                seenBlocks[graph.index(loop[j])] = true;
                blockWorklist.append(loop[j]);
            }
        
            while (!blockWorklist.isEmpty()) {
                typename Graph::Node block = blockWorklist.takeLast();
            
                if (verbose)
                    dataLog("    Dealing with ", graph.dump(block), "\n");
            
                if (block == loop.header())
                    continue;
            
                for (unsigned j = graph.predecessors(block).size(); j--;) {
                    typename Graph::Node predecessor = graph.predecessors(block)[j];
                    if (seenBlocks[graph.index(predecessor)])
                        continue;
                
                    loop.addBlock(predecessor);
                    blockWorklist.append(predecessor);
                    seenBlocks[graph.index(predecessor)] = true;
                }
            }
        }

        // Figure out reverse mapping from blocks to loops.
        for (unsigned blockIndex = graph.numNodes(); blockIndex--;) {
            typename Graph::Node block = graph.node(blockIndex);
            if (!block)
                continue;
            for (unsigned i = std::tuple_size<InnerMostLoopIndices>::value; i--;)
                m_innerMostLoopIndices[block][i] = UINT_MAX;
        }
        for (unsigned loopIndex = m_loops.size(); loopIndex--;) {
            NaturalLoop<Graph>& loop = m_loops[loopIndex];
        
            for (unsigned blockIndexInLoop = loop.size(); blockIndexInLoop--;) {
                typename Graph::Node block = loop[blockIndexInLoop];
            
                for (unsigned i = 0; i < std::tuple_size<InnerMostLoopIndices>::value; ++i) {
                    unsigned thisIndex = m_innerMostLoopIndices[block][i];
                    if (thisIndex == UINT_MAX || loop.size() < m_loops[thisIndex].size()) {
                        insertIntoBoundedVector(
                            m_innerMostLoopIndices[block], std::tuple_size<InnerMostLoopIndices>::value,
                            loopIndex, i);
                        break;
                    }
                }
            }
        }
    
        // Now each block knows its inner-most loop and its next-to-inner-most loop. Use
        // this to figure out loop parenting.
        for (unsigned i = m_loops.size(); i--;) {
            NaturalLoop<Graph>& loop = m_loops[i];
            RELEASE_ASSERT(m_innerMostLoopIndices[loop.header()][0] == i);
        
            loop.m_outerLoopIndex = m_innerMostLoopIndices[loop.header()][1];
        }
    
        if (selfCheck) {
            // Do some self-verification that we've done some of this correctly.
        
            for (unsigned blockIndex = graph.numNodes(); blockIndex--;) {
                typename Graph::Node block = graph.node(blockIndex);
                if (!block)
                    continue;
            
                Vector<const NaturalLoop<Graph>*> simpleLoopsOf;
            
                for (unsigned i = m_loops.size(); i--;) {
                    if (m_loops[i].contains(block))
                        simpleLoopsOf.append(&m_loops[i]);
                }
            
                Vector<const NaturalLoop<Graph>*> fancyLoopsOf = loopsOf(block);
            
                std::sort(simpleLoopsOf.begin(), simpleLoopsOf.end());
                std::sort(fancyLoopsOf.begin(), fancyLoopsOf.end());
            
                RELEASE_ASSERT(simpleLoopsOf == fancyLoopsOf);
            }
        }
    
        if (verbose)
            dataLog("Results: ", *this, "\n");
    }
    
    Graph& graph() { return m_graph; }
    
    unsigned numLoops() const
    {
        return m_loops.size();
    }
    const NaturalLoop<Graph>& loop(unsigned i) const
    {
        return m_loops[i];
    }
    
    // Return either null if the block isn't a loop header, or the
    // loop it belongs to.
    const NaturalLoop<Graph>* headerOf(typename Graph::Node block) const
    {
        const NaturalLoop<Graph>* loop = innerMostLoopOf(block);
        if (!loop)
            return nullptr;
        if (loop->header() == block)
            return loop;
        if (!ASSERT_DISABLED) {
            for (; loop; loop = innerMostOuterLoop(*loop))
                ASSERT(loop->header() != block);
        }
        return nullptr;
    }
    
    const NaturalLoop<Graph>* innerMostLoopOf(typename Graph::Node block) const
    {
        unsigned index = m_innerMostLoopIndices[block][0];
        if (index == UINT_MAX)
            return nullptr;
        return &m_loops[index];
    }
    
    const NaturalLoop<Graph>* innerMostOuterLoop(const NaturalLoop<Graph>& loop) const
    {
        if (loop.m_outerLoopIndex == UINT_MAX)
            return nullptr;
        return &m_loops[loop.m_outerLoopIndex];
    }
    
    bool belongsTo(typename Graph::Node block, const NaturalLoop<Graph>& candidateLoop) const
    {
        // It's faster to do this test using the loop itself, if it's small.
        if (candidateLoop.size() < 4)
            return candidateLoop.contains(block);
        
        for (const NaturalLoop<Graph>* loop = innerMostLoopOf(block); loop; loop = innerMostOuterLoop(*loop)) {
            if (loop == &candidateLoop)
                return true;
        }
        return false;
    }
    
    unsigned loopDepth(typename Graph::Node block) const
    {
        unsigned depth = 0;
        for (const NaturalLoop<Graph>* loop = innerMostLoopOf(block); loop; loop = innerMostOuterLoop(*loop))
            depth++;
        return depth;
    }
    
    // Return all loops this belongs to. The first entry in the vector is the innermost loop. The last is the
    // outermost loop.
    Vector<const NaturalLoop<Graph>*> loopsOf(typename Graph::Node block) const
    {
        Vector<const NaturalLoop<Graph>*> result;
        for (const NaturalLoop<Graph>* loop = innerMostLoopOf(block); loop; loop = innerMostOuterLoop(*loop))
            result.append(loop);
        return result;
    }

    void dump(PrintStream& out) const
    {
        out.print("NaturalLoops:{");
        CommaPrinter comma;
        for (unsigned i = 0; i < m_loops.size(); ++i)
            out.print(comma, m_loops[i]);
        out.print("}");
    }
    
private:
    Graph& m_graph;
    
    // If we ever had a scalability problem in our natural loop finder, we could
    // use some HashMap's here. But it just feels a heck of a lot less convenient.
    Vector<NaturalLoop<Graph>, 4> m_loops;
    
    typename Graph::template Map<InnerMostLoopIndices> m_innerMostLoopIndices;
};

} // namespace WTF

using WTF::NaturalLoop;
using WTF::NaturalLoops;
