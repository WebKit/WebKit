/*
 * Copyright (C) 2016 Yusuke Suzuki <utatane.tea@gmail.com>
 * Copyright (C) 2016-2021 Apple Inc. All rights reserved.
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
#include "BytecodeGeneratorification.h"

#include "BytecodeDumper.h"
#include "BytecodeGeneratorBaseInlines.h"
#include "BytecodeLivenessAnalysisInlines.h"
#include "BytecodeRewriter.h"
#include "BytecodeStructs.h"
#include "BytecodeUseDef.h"
#include "JSGenerator.h"
#include "Label.h"
#include "StrongInlines.h"
#include "UnlinkedCodeBlockGenerator.h"
#include "UnlinkedMetadataTableInlines.h"
#include <wtf/HashMap.h>

namespace JSC {

struct YieldData {
    JSInstructionStream::Offset point { 0 };
    VirtualRegister argument { 0 };
    BitVector liveness;
};

class BytecodeGeneratorification {
public:
    typedef Vector<YieldData> Yields;

    struct GeneratorFrameData {
        JSInstructionStream::Offset m_point;
        VirtualRegister m_dst;
        VirtualRegister m_scope;
        VirtualRegister m_symbolTable;
        VirtualRegister m_initialValue;
    };

    BytecodeGeneratorification(BytecodeGenerator& bytecodeGenerator, UnlinkedCodeBlockGenerator* codeBlock, JSInstructionStreamWriter& instructions, SymbolTable* generatorFrameSymbolTable)
        : m_bytecodeGenerator(bytecodeGenerator)
        , m_codeBlock(codeBlock)
        , m_instructions(instructions)
        , m_graph(m_codeBlock, m_instructions)
        , m_generatorFrameSymbolTable(codeBlock->vm(), generatorFrameSymbolTable)
    {
        for (const auto& instruction : m_instructions) {
            switch (instruction->opcodeID()) {
            case op_enter: {
                m_enterPoint = instruction.offset();
                break;
            }

            case op_yield: {
                auto bytecode = instruction->as<OpYield>();
                unsigned liveCalleeLocalsIndex = bytecode.m_yieldPoint;
                if (liveCalleeLocalsIndex >= m_yields.size())
                    m_yields.grow(liveCalleeLocalsIndex + 1);
                YieldData& data = m_yields[liveCalleeLocalsIndex];
                data.point = instruction.offset();
                data.argument = bytecode.m_argument;
                break;
            }

            case op_create_generator_frame_environment: {
                auto bytecode = instruction->as<OpCreateGeneratorFrameEnvironment>();
                GeneratorFrameData data;
                data.m_point = instruction.offset();
                data.m_dst = bytecode.m_dst;
                data.m_scope = bytecode.m_scope;
                data.m_symbolTable = bytecode.m_symbolTable;
                data.m_initialValue = bytecode.m_initialValue;
                m_generatorFrameData = WTF::move(data);
                break;
            }

            default:
                break;
            }
        }
    }

    void run();

    BytecodeGraph& NODELETE graph() { return m_graph; }

    const Yields& NODELETE yields() const
    {
        return m_yields;
    }

    Yields& NODELETE yields()
    {
        return m_yields;
    }

    JSInstructionStream::Ref NODELETE enterPoint() const
    {
        return m_instructions.at(m_enterPoint);
    }

    std::optional<GeneratorFrameData> NODELETE generatorFrameData() const
    {
        return m_generatorFrameData;
    }

    const JSInstructionStream& NODELETE instructions() const
    {
        return m_instructions;
    }

private:
    BytecodeGenerator& m_bytecodeGenerator;
    JSInstructionStream::Offset m_enterPoint;
    std::optional<GeneratorFrameData> m_generatorFrameData;
    UnlinkedCodeBlockGenerator* m_codeBlock;
    JSInstructionStreamWriter& m_instructions;
    BytecodeGraph m_graph;
    Yields m_yields;
    Strong<SymbolTable> m_generatorFrameSymbolTable;
};

class GeneratorLivenessAnalysis : public BytecodeLivenessPropagation {
public:
    GeneratorLivenessAnalysis(BytecodeGeneratorification& generatorification)
        : m_generatorification(generatorification)
    {
    }

    void run(UnlinkedCodeBlockGenerator* codeBlock, JSInstructionStreamWriter& instructions)
    {
        // Perform modified liveness analysis to determine which locals are live at the merge points.
        // This produces the conservative results for the question, "which variables should be saved and resumed?".

        runLivenessFixpoint(codeBlock, instructions, m_generatorification.graph());

        for (YieldData& data : m_generatorification.yields()) {
            FastBitVector liveness = getLivenessInfoAtInstruction(codeBlock, instructions, m_generatorification.graph(), BytecodeIndex(m_generatorification.instructions().at(data.point).next().offset()));
            liveness.forEachSetBit([&](size_t index) {
                data.liveness.set(index);
            });
        }
    }

private:
    BytecodeGeneratorification& m_generatorification;
};

void BytecodeGeneratorification::run()
{
    // We calculate the liveness at each merge point. This gives us the information which registers should be saved and resumed conservatively.

    {
        GeneratorLivenessAnalysis pass(*this);
        pass.run(m_codeBlock, m_instructions);
    }

    BytecodeRewriter rewriter(m_bytecodeGenerator, m_graph, m_codeBlock, m_instructions);

    // Setup the global switch for the generator.
    {
        auto nextToEnterPoint = enterPoint().next();
        unsigned switchTableIndex = m_codeBlock->numberOfUnlinkedSwitchJumpTables();
        VirtualRegister state = virtualRegisterForArgumentIncludingThis(static_cast<int32_t>(JSGenerator::Argument::State));
        auto& jumpTable = m_codeBlock->addUnlinkedSwitchJumpTable();
        jumpTable.m_min = 0;
        jumpTable.m_branchOffsets = FixedVector<int32_t>(m_yields.size() + 1);
        std::ranges::fill(jumpTable.m_branchOffsets, 0);
        jumpTable.add(0, nextToEnterPoint.offset());
        for (unsigned i = 0; i < m_yields.size(); ++i)
            jumpTable.add(i + 1, m_yields[i].point);
        jumpTable.m_defaultOffset = nextToEnterPoint.offset();

        rewriter.insertFragmentBefore(nextToEnterPoint, [&] (BytecodeRewriter::Fragment& fragment) {
            fragment.appendInstruction<OpSwitchImm>(switchTableIndex, state);
        });
    }

    unsigned firstScopeOffset = m_generatorFrameSymbolTable->scopeSize();
    unsigned maxLiveLocals = 0;
    for (const YieldData& data : m_yields)
        maxLiveLocals = std::max<unsigned>(maxLiveLocals, data.liveness.bitCount());
    if (maxLiveLocals)
        m_generatorFrameSymbolTable->didUseScopeOffset(ScopeOffset(firstScopeOffset + maxLiveLocals - 1));

    UncheckedKeyHashMap<BitVector, unsigned> bitVectorIndices;
    VirtualRegister scope = virtualRegisterForArgumentIncludingThis(static_cast<int32_t>(JSGenerator::Argument::Frame));
    for (const YieldData& data : m_yields) {
        auto instruction = m_instructions.at(data.point);
        unsigned liveLocals = data.liveness.bitCount();
        unsigned liveLocalsIndex = 0;
        unsigned firstValueProfile = 0;
        if (liveLocals) {
            liveLocalsIndex = bitVectorIndices.ensure(data.liveness, [&] {
                return m_codeBlock->addBitVector(BitVector(data.liveness));
            }).iterator->value;
            firstValueProfile = m_bytecodeGenerator.nextValueProfileIndex();
            for (unsigned i = 1; i < liveLocals; ++i)
                m_bytecodeGenerator.nextValueProfileIndex();
        }

        rewriter.insertFragmentBefore(instruction, [&] (BytecodeRewriter::Fragment& fragment) {
            if (liveLocals)
                fragment.appendInstruction<OpSaveGeneratorLocals>(scope, liveLocalsIndex, firstScopeOffset, maxLiveLocals);
            fragment.appendInstruction<OpRet>(data.argument);
        });

        rewriter.replaceBytecodeWithFragment(instruction, [&] (BytecodeRewriter::Fragment& fragment) {
            if (liveLocals)
                fragment.appendInstruction<OpRestoreGeneratorLocals>(scope, liveLocalsIndex, firstScopeOffset, firstValueProfile);
        });
    }

    if (m_generatorFrameData) {
        auto instruction = m_instructions.at(m_generatorFrameData->m_point);
        rewriter.replaceBytecodeWithFragment(instruction, [&] (BytecodeRewriter::Fragment& fragment) {
            if (!m_generatorFrameSymbolTable->scopeSize()) {
                // This will cause us to put jsUndefined() into the generator frame's scope value.
                fragment.appendInstruction<OpMov>(m_generatorFrameData->m_dst, m_generatorFrameData->m_initialValue);
            } else
                fragment.appendInstruction<OpCreateLexicalEnvironment>(m_generatorFrameData->m_dst, m_generatorFrameData->m_scope, m_generatorFrameData->m_symbolTable, m_generatorFrameData->m_initialValue);
        });
    }

    rewriter.execute();
}

void performGeneratorification(BytecodeGenerator& bytecodeGenerator, UnlinkedCodeBlockGenerator* codeBlock, JSInstructionStreamWriter& instructions, SymbolTable* generatorFrameSymbolTable)
{
    if (Options::dumpBytecodesBeforeGeneratorification()) [[unlikely]] {
        dataLogLn("Bytecodes before generatorification");
        CodeBlockBytecodeDumper<UnlinkedCodeBlockGenerator>::dumpBlock(codeBlock, instructions, WTF::dataFile());
    }

    BytecodeGeneratorification pass(bytecodeGenerator, codeBlock, instructions, generatorFrameSymbolTable);
    pass.run();

    if (Options::dumpBytecodesBeforeGeneratorification()) [[unlikely]] {
        dataLogLn("Bytecodes after generatorification");
        CodeBlockBytecodeDumper<UnlinkedCodeBlockGenerator>::dumpBlock(codeBlock, instructions, WTF::dataFile());
    }
}

} // namespace JSC
