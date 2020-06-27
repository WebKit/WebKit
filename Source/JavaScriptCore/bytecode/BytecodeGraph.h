/*
 * Copyright (C) 2016 Yusuke Suzuki <utatane.tea@gmail.com>
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#pragma once

#include "BytecodeBasicBlock.h"
#include "BytecodeDumper.h"
#include <wtf/IndexedContainerIterator.h>
#include <wtf/IteratorRange.h>
#include <wtf/Vector.h>

namespace JSC {

class BytecodeBasicBlock;

class BytecodeGraph {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(BytecodeGraph);
public:
    using BasicBlocksVector = BytecodeBasicBlock::BasicBlockVector;

    typedef WTF::IndexedContainerIterator<BytecodeGraph> iterator;

    template <typename CodeBlockType>
    inline BytecodeGraph(CodeBlockType*, const InstructionStream&);

    WTF::IteratorRange<BasicBlocksVector::reverse_iterator> basicBlocksInReverseOrder()
    {
        return WTF::makeIteratorRange(m_basicBlocks.rbegin(), m_basicBlocks.rend());
    }

    static bool blockContainsBytecodeOffset(const BytecodeBasicBlock& block, InstructionStream::Offset bytecodeOffset)
    {
        unsigned leaderOffset = block.leaderOffset();
        return bytecodeOffset >= leaderOffset && bytecodeOffset < leaderOffset + block.totalLength();
    }

    BytecodeBasicBlock* findBasicBlockForBytecodeOffset(InstructionStream::Offset bytecodeOffset)
    {
        /*
            for (unsigned i = 0; i < m_basicBlocks.size(); i++) {
                if (blockContainsBytecodeOffset(m_basicBlocks[i], bytecodeOffset))
                    return &m_basicBlocks[i];
            }
            return 0;
        */

        BytecodeBasicBlock* basicBlock = approximateBinarySearch<BytecodeBasicBlock, unsigned>(m_basicBlocks, m_basicBlocks.size(), bytecodeOffset, [] (BytecodeBasicBlock* basicBlock) { return basicBlock->leaderOffset(); });
        // We found the block we were looking for.
        if (blockContainsBytecodeOffset(*basicBlock, bytecodeOffset))
            return basicBlock;

        // Basic block is to the left of the returned block.
        if (bytecodeOffset < basicBlock->leaderOffset()) {
            ASSERT(basicBlock - 1 >= m_basicBlocks.data());
            ASSERT(blockContainsBytecodeOffset(basicBlock[-1], bytecodeOffset));
            return &basicBlock[-1];
        }

        // Basic block is to the right of the returned block.
        ASSERT(&basicBlock[1] <= &m_basicBlocks.last());
        ASSERT(blockContainsBytecodeOffset(basicBlock[1], bytecodeOffset));
        return &basicBlock[1];
    }

    BytecodeBasicBlock* findBasicBlockWithLeaderOffset(InstructionStream::Offset leaderOffset)
    {
        return tryBinarySearch<BytecodeBasicBlock, unsigned>(m_basicBlocks, m_basicBlocks.size(), leaderOffset, [] (BytecodeBasicBlock* basicBlock) { return basicBlock->leaderOffset(); });
    }

    unsigned size() const { return m_basicBlocks.size(); }
    BytecodeBasicBlock& at(unsigned index) const { return const_cast<BytecodeGraph*>(this)->m_basicBlocks[index]; }
    BytecodeBasicBlock& operator[](unsigned index) const { return at(index); }

    iterator begin() { return iterator(*this, 0); }
    iterator end() { return iterator(*this, size()); }
    BytecodeBasicBlock& first() { return at(0); }
    BytecodeBasicBlock& last() { return at(size() - 1); }


    template <typename CodeBlockType>
    void dump(CodeBlockType* codeBlock, const InstructionStream& instructions, Optional<Vector<Operands<SpeculatedType>>> speculationAtHead, PrintStream& printer = WTF::dataFile())
    {
        CodeBlockBytecodeDumper<CodeBlockType>::dumpGraph(codeBlock, instructions, *this, speculationAtHead, printer);
    }

private:
    BasicBlocksVector m_basicBlocks;
};


template<typename CodeBlockType>
BytecodeGraph::BytecodeGraph(CodeBlockType* codeBlock, const InstructionStream& instructions)
    : m_basicBlocks(BytecodeBasicBlock::compute(codeBlock, instructions))
{
    ASSERT(m_basicBlocks.size());
}

} // namespace JSC
