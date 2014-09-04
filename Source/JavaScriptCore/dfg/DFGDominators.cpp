/*
 * Copyright (C) 2011, 2014 Apple Inc. All rights reserved.
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

#include "config.h"
#include "DFGDominators.h"

#if ENABLE(DFG_JIT)

#include "DFGBlockMapInlines.h"
#include "DFGBlockWorklist.h"
#include "DFGGraph.h"
#include "DFGNaiveDominators.h"
#include "JSCInlines.h"

namespace JSC { namespace DFG {

Dominators::Dominators()
{
}

Dominators::~Dominators()
{
}

namespace {

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
public:
    LengauerTarjan(Graph& graph)
        : m_graph(graph)
        , m_data(graph)
    {
        for (BlockIndex blockIndex = m_graph.numBlocks(); blockIndex--;) {
            BasicBlock* block = m_graph.block(blockIndex);
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
    
    BasicBlock* immediateDominator(BasicBlock* block)
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
        
        ExtendedBlockWorklist<unsigned> worklist;
        worklist.push(m_graph.block(0), 0);
        
        while (BlockWith<unsigned> item = worklist.pop()) {
            BasicBlock* block = item.block;
            unsigned successorIndex = item.data;
            
            // We initially push with successorIndex = 0 regardless of whether or not we have any
            // successors. This is so that we can assign our prenumber. Subsequently we get pushed
            // with higher successorIndex values, but only if they are in range.
            ASSERT(!successorIndex || successorIndex < block->numSuccessors());

            if (!successorIndex) {
                m_data[block].semiNumber = m_blockByPreNumber.size();
                m_blockByPreNumber.append(block);
            }
            
            if (successorIndex < block->numSuccessors()) {
                unsigned nextSuccessorIndex = successorIndex + 1;
                if (nextSuccessorIndex < block->numSuccessors())
                    worklist.forcePush(block, nextSuccessorIndex);

                BasicBlock* successorBlock = block->successor(successorIndex);
                if (worklist.push(successorBlock, 0))
                    m_data[successorBlock].parent = block;
            }
        }
    }
    
    void computeSemiDominatorsAndImplicitImmediateDominators()
    {
        for (unsigned currentPreNumber = m_blockByPreNumber.size(); currentPreNumber-- > 1;) {
            BasicBlock* block = m_blockByPreNumber[currentPreNumber];
            BlockData& blockData = m_data[block];
            
            // Step 2:
            for (BasicBlock* predecessorBlock : block->predecessors) {
                BasicBlock* intermediateBlock = eval(predecessorBlock);
                blockData.semiNumber = std::min(
                    m_data[intermediateBlock].semiNumber, blockData.semiNumber);
            }
            unsigned bucketPreNumber = blockData.semiNumber;
            ASSERT(bucketPreNumber <= currentPreNumber);
            m_data[m_blockByPreNumber[bucketPreNumber]].bucket.append(block);
            link(blockData.parent, block);
            
            // Step 3:
            for (BasicBlock* semiDominee : m_data[blockData.parent].bucket) {
                BasicBlock* possibleDominator = eval(semiDominee);
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
            BasicBlock* block = m_blockByPreNumber[currentPreNumber];
            BlockData& blockData = m_data[block];
            
            if (blockData.dom != m_blockByPreNumber[blockData.semiNumber])
                blockData.dom = m_data[blockData.dom].dom;
        }
    }
    
    void link(BasicBlock* from, BasicBlock* to)
    {
        m_data[to].ancestor = from;
    }
    
    BasicBlock* eval(BasicBlock* block)
    {
        if (!m_data[block].ancestor)
            return block;
        
        compress(block);
        return m_data[block].label;
    }
    
    void compress(BasicBlock* initialBlock)
    {
        // This was meant to be a recursive function, but we don't like recursion because we don't
        // want to blow the stack. The original function will call compress() recursively on the
        // ancestor of anything that has an ancestor. So, we populate our worklist with the
        // recursive ancestors of initialBlock. Then we process the list starting from the block
        // that is furthest up the ancestor chain.
        
        BasicBlock* ancestor = m_data[initialBlock].ancestor;
        ASSERT(ancestor);
        if (!m_data[ancestor].ancestor)
            return;
        
        Vector<BasicBlock*, 16> stack;
        for (BasicBlock* block = initialBlock; block; block = m_data[block].ancestor)
            stack.append(block);
        
        // We only care about blocks that have an ancestor that has an ancestor. The last two
        // elements in the stack won't satisfy this property.
        ASSERT(stack.size() >= 2);
        ASSERT(!m_data[stack[stack.size() - 1]].ancestor);
        ASSERT(!m_data[m_data[stack[stack.size() - 2]].ancestor].ancestor);
        
        for (unsigned i = stack.size() - 2; i--;) {
            BasicBlock* block = stack[i];
            BasicBlock*& labelOfBlock = m_data[block].label;
            BasicBlock*& ancestorOfBlock = m_data[block].ancestor;
            ASSERT(ancestorOfBlock);
            ASSERT(m_data[ancestorOfBlock].ancestor);
            
            BasicBlock* labelOfAncestorOfBlock = m_data[ancestorOfBlock].label;
            
            if (m_data[labelOfAncestorOfBlock].semiNumber < m_data[labelOfBlock].semiNumber)
                labelOfBlock = labelOfAncestorOfBlock;
            ancestorOfBlock = m_data[ancestorOfBlock].ancestor;
        }
    }

    struct BlockData {
        BlockData()
            : parent(nullptr)
            , preNumber(UINT_MAX)
            , semiNumber(UINT_MAX)
            , ancestor(nullptr)
            , label(nullptr)
            , dom(nullptr)
        {
        }
        
        BasicBlock* parent;
        unsigned preNumber;
        unsigned semiNumber;
        BasicBlock* ancestor;
        BasicBlock* label;
        Vector<BasicBlock*> bucket;
        BasicBlock* dom;
    };
    
    Graph& m_graph;
    BlockMap<BlockData> m_data;
    Vector<BasicBlock*> m_blockByPreNumber;
};

struct ValidationContext {
    ValidationContext(Graph& graph, Dominators& dominators)
        : graph(graph)
        , dominators(dominators)
    {
    }
    
    void reportError(BasicBlock* from, BasicBlock* to, const char* message)
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
        
        startCrashing();
        dataLog("DFG DOMINATOR VALIDATION FAILED:\n");
        dataLog("\n");
        dataLog("For block domination relationships:\n");
        for (unsigned i = 0; i < errors.size(); ++i) {
            dataLog(
                "    ", pointerDump(errors[i].from), " -> ", pointerDump(errors[i].to),
                " (", errors[i].message, ")\n");
        }
        dataLog("\n");
        dataLog("Control flow graph:\n");
        for (BlockIndex blockIndex = 0; blockIndex < graph.numBlocks(); ++blockIndex) {
            BasicBlock* block = graph.block(blockIndex);
            if (!block)
                continue;
            dataLog("    Block #", blockIndex, ": successors = [");
            CommaPrinter comma;
            for (unsigned i = 0; i < block->numSuccessors(); ++i)
                dataLog(comma, *block->successor(i));
            dataLog("], predecessors = [");
            comma = CommaPrinter();
            for (unsigned i = 0; i < block->predecessors.size(); ++i)
                dataLog(comma, *block->predecessors[i]);
            dataLog("]\n");
        }
        dataLog("\n");
        dataLog("Lengauer-Tarjan Dominators:\n");
        dataLog(dominators);
        dataLog("\n");
        dataLog("Naive Dominators:\n");
        naiveDominators.dump(graph, WTF::dataFile());
        dataLog("\n");
        dataLog("Graph at time of failure:\n");
        graph.dump();
        dataLog("\n");
        dataLog("DFG DOMINATOR VALIDATION FAILIED!\n");
        CRASH();
    }
    
    Graph& graph;
    Dominators& dominators;
    NaiveDominators naiveDominators;
    
    struct Error {
        BasicBlock* from;
        BasicBlock* to;
        const char* message;
    };
    
    Vector<Error> errors;
};

} // anonymous namespace

void Dominators::compute(Graph& graph)
{
    LengauerTarjan lengauerTarjan(graph);
    lengauerTarjan.compute();

    m_data = BlockMap<BlockData>(graph);
    
    // From here we want to build a spanning tree with both upward and downward links and we want
    // to do a search over this tree to compute pre and post numbers that can be used for dominance
    // tests.
    
    for (BlockIndex blockIndex = graph.numBlocks(); blockIndex--;) {
        BasicBlock* block = graph.block(blockIndex);
        if (!block)
            continue;
        
        BasicBlock* idomBlock = lengauerTarjan.immediateDominator(block);
        m_data[block].idomParent = idomBlock;
        if (idomBlock)
            m_data[idomBlock].idomKids.append(block);
    }
    
    unsigned nextPreNumber = 0;
    unsigned nextPostNumber = 0;
    
    // Plain stack-based worklist because we are guaranteed to see each block exactly once anyway.
    Vector<BlockWithOrder> worklist;
    worklist.append(BlockWithOrder(graph.block(0), PreOrder));
    while (!worklist.isEmpty()) {
        BlockWithOrder item = worklist.takeLast();
        switch (item.order) {
        case PreOrder:
            m_data[item.block].preNumber = nextPreNumber++;
            worklist.append(BlockWithOrder(item.block, PostOrder));
            for (BasicBlock* kid : m_data[item.block].idomKids)
                worklist.append(BlockWithOrder(kid, PreOrder));
            break;
        case PostOrder:
            m_data[item.block].postNumber = nextPostNumber++;
            break;
        }
    }
    
    if (validationEnabled()) {
        // Check our dominator calculation:
        // 1) Check that our range-based ancestry test is the same as a naive ancestry test.
        // 2) Check that our notion of who dominates whom is identical to a naive (not
        //    Lengauer-Tarjan) dominator calculation.
        
        ValidationContext context(graph, *this);
        context.naiveDominators.compute(graph);
        
        for (BlockIndex fromBlockIndex = graph.numBlocks(); fromBlockIndex--;) {
            BasicBlock* fromBlock = graph.block(fromBlockIndex);
            if (!fromBlock || m_data[fromBlock].preNumber == UINT_MAX)
                continue;
            for (BlockIndex toBlockIndex = graph.numBlocks(); toBlockIndex--;) {
                BasicBlock* toBlock = graph.block(toBlockIndex);
                if (!toBlock || m_data[toBlock].preNumber == UINT_MAX)
                    continue;
                
                if (dominates(fromBlock, toBlock) != naiveDominates(fromBlock, toBlock))
                    context.reportError(fromBlock, toBlock, "Range-based domination check is broken");
                if (dominates(fromBlock, toBlock) != context.naiveDominators.dominates(fromBlock, toBlock))
                    context.reportError(fromBlock, toBlock, "Lengauer-Tarjan domination is broken");
            }
        }
        
        context.handleErrors();
    }
}

BlockSet Dominators::strictDominatorsOf(BasicBlock* to) const
{
    BlockSet result;
    forAllStrictDominatorsOf(to, BlockAdder(result));
    return result;
}

BlockSet Dominators::dominatorsOf(BasicBlock* to) const
{
    BlockSet result;
    forAllDominatorsOf(to, BlockAdder(result));
    return result;
}

BlockSet Dominators::blocksStrictlyDominatedBy(BasicBlock* from) const
{
    BlockSet result;
    forAllBlocksStrictlyDominatedBy(from, BlockAdder(result));
    return result;
}

BlockSet Dominators::blocksDominatedBy(BasicBlock* from) const
{
    BlockSet result;
    forAllBlocksDominatedBy(from, BlockAdder(result));
    return result;
}

BlockSet Dominators::dominanceFrontierOf(BasicBlock* from) const
{
    BlockSet result;
    forAllBlocksInDominanceFrontierOfImpl(from, BlockAdder(result));
    return result;
}

BlockSet Dominators::iteratedDominanceFrontierOf(const BlockList& from) const
{
    BlockSet result;
    forAllBlocksInIteratedDominanceFrontierOfImpl(from, BlockAdder(result));
    return result;
}

bool Dominators::naiveDominates(BasicBlock* from, BasicBlock* to) const
{
    for (BasicBlock* block = to; block; block = m_data[block].idomParent) {
        if (block == from)
            return true;
    }
    return false;
}

void Dominators::dump(PrintStream& out) const
{
    if (!isValid()) {
        out.print("    Not Valid.\n");
        return;
    }
    
    for (BlockIndex blockIndex = 0; blockIndex < m_data.size(); ++blockIndex) {
        if (m_data[blockIndex].preNumber == UINT_MAX)
            continue;
        
        out.print("    Block #", blockIndex, ": idom = ", pointerDump(m_data[blockIndex].idomParent), ", idomKids = [");
        CommaPrinter comma;
        for (unsigned i = 0; i < m_data[blockIndex].idomKids.size(); ++i)
            out.print(comma, *m_data[blockIndex].idomKids[i]);
        out.print("], pre/post = ", m_data[blockIndex].preNumber, "/", m_data[blockIndex].postNumber, "\n");
    }
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

