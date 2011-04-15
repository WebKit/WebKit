/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#ifndef DFGGraph_h
#define DFGGraph_h

#if ENABLE(DFG_JIT)

#include <dfg/DFGNode.h>
#include <wtf/Vector.h>
#include <wtf/StdLibExtras.h>

namespace JSC {

class CodeBlock;

namespace DFG {

typedef uint32_t BlockIndex;

struct BasicBlock {
    BasicBlock(unsigned bytecodeBegin, NodeIndex begin, NodeIndex end)
        : bytecodeBegin(bytecodeBegin)
        , begin(begin)
        , end(end)
    {
    }

    static inline BlockIndex getBytecodeBegin(BasicBlock* block)
    {
        return block->bytecodeBegin;
    }

    unsigned bytecodeBegin;
    NodeIndex begin;
    NodeIndex end;
};

// 
// === Graph ===
//
// The dataflow graph is an ordered vector of nodes.
// The order may be significant for nodes with side-effects (property accesses, value conversions).
// Nodes that are 'dead' remain in the vector with refCount 0.
class Graph : public Vector<Node, 64> {
public:
    // Mark a node as being referenced.
    void ref(NodeIndex nodeIndex)
    {
        Node& node = at(nodeIndex);
        // If the value (before incrementing) was at refCount zero then we need to ref its children.
        if (!node.refCount++)
            refChildren(nodeIndex);
    }
    void deref(NodeIndex nodeIndex)
    {
        Node& node = at(nodeIndex);
        ASSERT(node.refCount);
        // If the value (after decrementing) becomes refCount zero then we need to deref its children.
        if (!--node.refCount)
            derefChildren(nodeIndex);
    }

#ifndef NDEBUG
    // CodeBlock is optional, but may allow additional information to be dumped (e.g. Identifier names).
    void dump(CodeBlock* = 0);
    void dump(NodeIndex, CodeBlock* = 0);
#endif

    Vector<BasicBlock> m_blocks;

    BlockIndex blockIndexForBytecodeOffset(unsigned bytecodeBegin)
    {
        BasicBlock* begin = m_blocks.begin();
        BasicBlock* block = binarySearch<BasicBlock, unsigned, BasicBlock::getBytecodeBegin>(begin, m_blocks.size(), bytecodeBegin);
        ASSERT(block >= m_blocks.begin() && block < m_blocks.end());
        return static_cast<BlockIndex>(block - begin);
    }

private:
    // When a node's refCount goes from 0 to 1, it must (logically) recursively ref all of its children, and vice versa.
    void refChildren(NodeIndex);
    void derefChildren(NodeIndex);
};

} } // namespace JSC::DFG

#endif
#endif
