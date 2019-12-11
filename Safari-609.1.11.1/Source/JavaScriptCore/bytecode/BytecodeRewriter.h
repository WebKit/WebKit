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

#include "BytecodeGenerator.h"
#include "BytecodeGraph.h"
#include "BytecodeStructs.h"
#include "Bytecodes.h"
#include "Opcode.h"
#include "UnlinkedCodeBlock.h"
#include <wtf/Insertion.h>

namespace JSC {

// BytecodeRewriter offers the ability to insert and remove the bytecodes including jump operations.
//
// We use the original bytecode offsets as labels. When you emit some jumps, you can specify the jump target by
// using the original bytecode offsets. These bytecode offsets are later converted appropriate values by the
// rewriter. And we also use the labels to represents the position the new bytecodes inserted.
//
//                      |  [bytecode]  |  [bytecode]  |
//   offsets            A              B              C
//
// We can use the above "A", "B", and "C" offsets as labels. And the rewriter has the ability to insert bytecode fragments
// before and after the label. For example, if you insert the fragment after "B", the layout becomes like this.
//
//                      |  [bytecode]  |  [fragment]  [bytecode]  |
//   offsets            A              B                          C
//
//  And even if you remove some original bytecodes, the offset remains as labels. For example, when you remove the A's bytecode,
//  the layout becomes like this.
//
//                      |              |  [bytecode]  |
//   offsets            A              B              C
//
//  And still you can insert fragments before and after "A".
//
//                      |  [fragment]  |  [bytecode]  |
//   offsets            A              B              C
//
//   We can insert bytecode fragments "Before" and "After" the labels. This inserted position, either "Before" and "After",
//   has effect when the label is involved with jumps. For example, when you have jump to the position "B",
//
//                      |  [bytecode]  |  [bytecode]  |
//   offsets            A              B              C
//                                     ^
//                                     jump to here.
//
//  and you insert the bytecode before/after "B",
//
//                      |  [bytecode] [before]  |  [after] [bytecode]  |
//   offsets            A                       B              C
//                                              ^
//                                              jump to here.
//
//  as you can see, the execution jumping into "B" does not execute [before] code.
class BytecodeRewriter {
WTF_MAKE_NONCOPYABLE(BytecodeRewriter);
public:
    enum class Position : int8_t {
        EntryPoint = -2,
        Before = -1,
        LabelPoint = 0,
        After = 1,
        OriginalBytecodePoint = 2,
    };

    enum class IncludeBranch : uint8_t {
        No = 0,
        Yes = 1,
    };

    struct InsertionPoint {
        int32_t bytecodeOffset;
        Position position;

        InsertionPoint(InstructionStream::Offset offset, Position pos)
            : bytecodeOffset(offset)
            , position(pos)
        {
        }

        bool operator<(const InsertionPoint& other) const
        {
            if (bytecodeOffset == other.bytecodeOffset)
                return position < other.position;
            return bytecodeOffset < other.bytecodeOffset;
        }

        bool operator==(const InsertionPoint& other) const
        {
            return bytecodeOffset == other.bytecodeOffset && position == other.position;
        }
    };

private:
    struct Insertion {
        enum class Type : uint8_t { Insert = 0, Remove = 1, };

        size_t length() const
        {
            if (type == Type::Remove)
                return removeLength;
            return instructions.size();
        }

        InsertionPoint index;
        Type type;
        IncludeBranch includeBranch;
        size_t removeLength;
        InstructionStreamWriter instructions;
    };

public:
    class Fragment {
    WTF_MAKE_NONCOPYABLE(Fragment);
    public:
        Fragment(BytecodeGenerator& bytecodeGenerator, InstructionStreamWriter& writer, IncludeBranch& includeBranch)
            : m_bytecodeGenerator(bytecodeGenerator)
            , m_writer(writer)
            , m_includeBranch(includeBranch)
        {
        }

        template<class Op, class... Args>
        void appendInstruction(Args... args)
        {
            if (isBranch(Op::opcodeID))
                m_includeBranch = IncludeBranch::Yes;

            m_bytecodeGenerator.withWriter(m_writer, [&] {
                Op::emit(&m_bytecodeGenerator, std::forward<Args>(args)...);
            });
        }

        void align()
        {
#if CPU(NEEDS_ALIGNED_ACCESS)
            m_bytecodeGenerator.withWriter(m_writer, [&] {
                while (m_bytecodeGenerator.instructions().size() % OpcodeSize::Wide32)
                    OpNop::emit<OpcodeSize::Narrow>(&m_bytecodeGenerator);
            });
#endif
        }

    private:
        BytecodeGenerator& m_bytecodeGenerator;
        InstructionStreamWriter& m_writer;
        IncludeBranch& m_includeBranch;
    };

    BytecodeRewriter(BytecodeGenerator& bytecodeGenerator, BytecodeGraph& graph, UnlinkedCodeBlock* codeBlock, InstructionStreamWriter& writer)
        : m_bytecodeGenerator(bytecodeGenerator)
        , m_graph(graph)
        , m_codeBlock(codeBlock)
        , m_writer(writer)
    {
    }

    template<class Function>
    void insertFragmentBefore(const InstructionStream::Ref& instruction, Function function)
    {
        IncludeBranch includeBranch = IncludeBranch::No;
        InstructionStreamWriter writer;
        Fragment fragment(m_bytecodeGenerator, writer, includeBranch);
        function(fragment);
        fragment.align();
        insertImpl(InsertionPoint(instruction.offset(), Position::Before), includeBranch, WTFMove(writer));
    }

    template<class Function>
    void insertFragmentAfter(const InstructionStream::Ref& instruction, Function function)
    {
        IncludeBranch includeBranch = IncludeBranch::No;
        InstructionStreamWriter writer;
        Fragment fragment(m_bytecodeGenerator, writer, includeBranch);
        function(fragment);
        fragment.align();
        insertImpl(InsertionPoint(instruction.offset(), Position::After), includeBranch, WTFMove(writer));
    }

    void removeBytecode(const InstructionStream::Ref& instruction)
    {
        m_insertions.append(Insertion { InsertionPoint(instruction.offset(), Position::OriginalBytecodePoint), Insertion::Type::Remove, IncludeBranch::No, instruction->size(), { } });
    }

    void execute();

    BytecodeGraph& graph() { return m_graph; }

    int32_t adjustAbsoluteOffset(InstructionStream::Offset absoluteOffset)
    {
        return adjustJumpTarget(InsertionPoint(0, Position::EntryPoint), InsertionPoint(absoluteOffset, Position::LabelPoint));
    }

    int32_t adjustJumpTarget(InstructionStream::Offset originalBytecodeOffset, int32_t originalJumpTarget)
    {
        return adjustJumpTarget(InsertionPoint(originalBytecodeOffset, Position::LabelPoint), InsertionPoint(originalJumpTarget, Position::LabelPoint));
    }

    void adjustJumpTargets();

private:
    void insertImpl(InsertionPoint, IncludeBranch, InstructionStreamWriter&& fragment);

    friend class UnlinkedCodeBlock;
    void applyModification();
    void adjustJumpTargetsInFragment(unsigned finalOffset, Insertion&);

    int adjustJumpTarget(InsertionPoint startPoint, InsertionPoint jumpTargetPoint);
    template<typename Iterator> int calculateDifference(Iterator begin, Iterator end);

    BytecodeGenerator& m_bytecodeGenerator;
    BytecodeGraph& m_graph;
    UnlinkedCodeBlock* m_codeBlock;
    InstructionStreamWriter& m_writer;
    Vector<Insertion, 8> m_insertions;
};

template<typename Iterator>
inline int BytecodeRewriter::calculateDifference(Iterator begin, Iterator end)
{
    int result = 0;
    for (; begin != end; ++begin) {
        if (begin->type == Insertion::Type::Remove)
            result -= begin->length();
        else
            result += begin->length();
    }
    return result;
}

} // namespace JSC
