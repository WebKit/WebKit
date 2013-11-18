/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef BytecodeBasicBlock_h
#define BytecodeBasicBlock_h

#include <limits.h>
#include <wtf/FastBitVector.h>
#include <wtf/HashMap.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

namespace JSC {

class CodeBlock;

class BytecodeBasicBlock : public RefCounted<BytecodeBasicBlock> {
public:
    enum SpecialBlockType { EntryBlock, ExitBlock };
    BytecodeBasicBlock(unsigned start, unsigned length);
    BytecodeBasicBlock(SpecialBlockType);

    bool isEntryBlock() { return !m_leaderBytecodeOffset && !m_totalBytecodeLength; }
    bool isExitBlock() { return m_leaderBytecodeOffset == UINT_MAX && m_totalBytecodeLength == UINT_MAX; }

    unsigned leaderBytecodeOffset() { return m_leaderBytecodeOffset; }
    unsigned totalBytecodeLength() { return m_totalBytecodeLength; }

    Vector<unsigned>& bytecodeOffsets() { return m_bytecodeOffsets; }
    void addBytecodeLength(unsigned);

    void addPredecessor(BytecodeBasicBlock* block) { m_predecessors.append(block); }
    void addSuccessor(BytecodeBasicBlock* block) { m_successors.append(block); }

    Vector<BytecodeBasicBlock*>& predecessors() { return m_predecessors; }
    Vector<BytecodeBasicBlock*>& successors() { return m_successors; }

    FastBitVector& in() { return m_in; }
    FastBitVector& out() { return m_out; }

private:
    unsigned m_leaderBytecodeOffset;
    unsigned m_totalBytecodeLength;

    Vector<unsigned> m_bytecodeOffsets;

    Vector<BytecodeBasicBlock*> m_predecessors;
    Vector<BytecodeBasicBlock*> m_successors;

    FastBitVector m_in;
    FastBitVector m_out;
};

void computeBytecodeBasicBlocks(CodeBlock*, Vector<RefPtr<BytecodeBasicBlock> >&);

inline BytecodeBasicBlock::BytecodeBasicBlock(unsigned start, unsigned length)
    : m_leaderBytecodeOffset(start)
    , m_totalBytecodeLength(length)
{
    m_bytecodeOffsets.append(m_leaderBytecodeOffset);
}

inline BytecodeBasicBlock::BytecodeBasicBlock(BytecodeBasicBlock::SpecialBlockType blockType)
    : m_leaderBytecodeOffset(blockType == BytecodeBasicBlock::EntryBlock ? 0 : UINT_MAX)
    , m_totalBytecodeLength(blockType == BytecodeBasicBlock::EntryBlock ? 0 : UINT_MAX)
{
}

inline void BytecodeBasicBlock::addBytecodeLength(unsigned bytecodeLength)
{
    m_bytecodeOffsets.append(m_leaderBytecodeOffset + m_totalBytecodeLength);
    m_totalBytecodeLength += bytecodeLength;
}

} // namespace JSC

#endif // BytecodeBasicBlock_h
