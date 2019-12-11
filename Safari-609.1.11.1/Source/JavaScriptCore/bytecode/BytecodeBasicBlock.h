/*
 * Copyright (C) 2013, 2015 Apple Inc. All rights reserved.
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

#include "InstructionStream.h"
#include <limits.h>
#include <wtf/FastBitVector.h>
#include <wtf/Vector.h>

namespace JSC {

class CodeBlock;
class UnlinkedCodeBlock;
struct Instruction;

class BytecodeBasicBlock {
    WTF_MAKE_FAST_ALLOCATED;
public:
    enum SpecialBlockType { EntryBlock, ExitBlock };
    BytecodeBasicBlock(const InstructionStream::Ref&);
    BytecodeBasicBlock(SpecialBlockType);
    void shrinkToFit();

    bool isEntryBlock() { return !m_leaderOffset && !m_totalLength; }
    bool isExitBlock() { return m_leaderOffset == UINT_MAX && m_totalLength == UINT_MAX; }

    unsigned leaderOffset() const { return m_leaderOffset; }
    unsigned totalLength() const { return m_totalLength; }

    const Vector<InstructionStream::Offset>& offsets() const { return m_offsets; }

    const Vector<BytecodeBasicBlock*>& successors() const { return m_successors; }

    FastBitVector& in() { return m_in; }
    FastBitVector& out() { return m_out; }

    unsigned index() const { return m_index; }

    static void compute(CodeBlock*, const InstructionStream& instructions, Vector<std::unique_ptr<BytecodeBasicBlock>>&);
    static void compute(UnlinkedCodeBlock*, const InstructionStream& instructions, Vector<std::unique_ptr<BytecodeBasicBlock>>&);

private:
    template<typename Block> static void computeImpl(Block* codeBlock, const InstructionStream& instructions, Vector<std::unique_ptr<BytecodeBasicBlock>>& basicBlocks);

    void addSuccessor(BytecodeBasicBlock* block) { m_successors.append(block); }

    void addLength(unsigned);

    InstructionStream::Offset m_leaderOffset;
    unsigned m_totalLength;
    unsigned m_index;

    Vector<InstructionStream::Offset> m_offsets;
    Vector<BytecodeBasicBlock*> m_successors;

    FastBitVector m_in;
    FastBitVector m_out;
};

inline BytecodeBasicBlock::BytecodeBasicBlock(const InstructionStream::Ref& instruction)
    : m_leaderOffset(instruction.offset())
    , m_totalLength(instruction->size())
{
    m_offsets.append(m_leaderOffset);
}

inline BytecodeBasicBlock::BytecodeBasicBlock(BytecodeBasicBlock::SpecialBlockType blockType)
    : m_leaderOffset(blockType == BytecodeBasicBlock::EntryBlock ? 0 : UINT_MAX)
    , m_totalLength(blockType == BytecodeBasicBlock::EntryBlock ? 0 : UINT_MAX)
{
}

inline void BytecodeBasicBlock::addLength(unsigned bytecodeLength)
{
    m_offsets.append(m_leaderOffset + m_totalLength);
    m_totalLength += bytecodeLength;
}

} // namespace JSC
