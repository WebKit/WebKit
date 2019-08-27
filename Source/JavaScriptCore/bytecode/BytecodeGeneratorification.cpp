/*
 * Copyright (C) 2016 Yusuke Suzuki <utatane.tea@gmail.com>
 * Copyright (C) 2016-2019 Apple Inc. All rights reserved.
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
#include "BytecodeLivenessAnalysisInlines.h"
#include "BytecodeRewriter.h"
#include "BytecodeStructs.h"
#include "BytecodeUseDef.h"
#include "IdentifierInlines.h"
#include "InterpreterInlines.h"
#include "JSCInlines.h"
#include "JSCJSValueInlines.h"
#include "JSGeneratorFunction.h"
#include "Label.h"
#include "StrongInlines.h"
#include "UnlinkedCodeBlock.h"
#include "UnlinkedMetadataTableInlines.h"
#include <wtf/Optional.h>

namespace JSC {

struct YieldData {
    InstructionStream::Offset point { 0 };
    VirtualRegister argument { 0 };
    FastBitVector liveness;
};

class BytecodeGeneratorification {
public:
    typedef Vector<YieldData> Yields;

    struct GeneratorFrameData {
        InstructionStream::Offset m_point;
        VirtualRegister m_dst;
        VirtualRegister m_scope;
        VirtualRegister m_symbolTable;
        VirtualRegister m_initialValue;
    };

    BytecodeGeneratorification(BytecodeGenerator& bytecodeGenerator, UnlinkedCodeBlock* codeBlock, InstructionStreamWriter& instructions, SymbolTable* generatorFrameSymbolTable, int generatorFrameSymbolTableIndex)
        : m_bytecodeGenerator(bytecodeGenerator)
        , m_codeBlock(codeBlock)
        , m_instructions(instructions)
        , m_graph(m_codeBlock, m_instructions)
        , m_generatorFrameSymbolTable(codeBlock->vm(), generatorFrameSymbolTable)
        , m_generatorFrameSymbolTableIndex(generatorFrameSymbolTableIndex)
    {
        for (BytecodeBasicBlock* block : m_graph) {
            for (const auto offset : block->offsets()) {
                const auto instruction = m_instructions.at(offset);
                switch (instruction->opcodeID()) {
                case op_enter: {
                    m_enterPoint = instruction.offset();
                    break;
                }

                case op_yield: {
                    auto bytecode = instruction->as<OpYield>();
                    unsigned liveCalleeLocalsIndex = bytecode.m_yieldPoint;
                    if (liveCalleeLocalsIndex >= m_yields.size())
                        m_yields.resize(liveCalleeLocalsIndex + 1);
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
                    m_generatorFrameData = WTFMove(data);
                    break;
                }

                default:
                    break;
                }
            }
        }
    }

    struct Storage {
        Identifier identifier;
        unsigned identifierIndex;
        ScopeOffset scopeOffset;
    };

    void run();

    BytecodeGraph& graph() { return m_graph; }

    const Yields& yields() const
    {
        return m_yields;
    }

    Yields& yields()
    {
        return m_yields;
    }

    InstructionStream::Ref enterPoint() const
    {
        return m_instructions.at(m_enterPoint);
    }

    Optional<GeneratorFrameData> generatorFrameData() const
    {
        return m_generatorFrameData;
    }

    const InstructionStream& instructions() const
    {
        return m_instructions;
    }

private:
    Storage storageForGeneratorLocal(VM& vm, unsigned index)
    {
        // We assign a symbol to a register. There is one-on-one corresponding between a register and a symbol.
        // By doing so, we allocate the specific storage to save the given register.
        // This allow us not to save all the live registers even if the registers are not overwritten from the previous resuming time.
        // It means that, the register can be retrieved even if the immediate previous op_save does not save it.

        if (m_storages.size() <= index)
            m_storages.resize(index + 1);
        if (Optional<Storage> storage = m_storages[index])
            return *storage;

        Identifier identifier = Identifier::from(vm, index);
        unsigned identifierIndex = m_codeBlock->numberOfIdentifiers();
        m_codeBlock->addIdentifier(identifier);
        ScopeOffset scopeOffset = m_generatorFrameSymbolTable->takeNextScopeOffset(NoLockingNecessary);
        m_generatorFrameSymbolTable->set(NoLockingNecessary, identifier.impl(), SymbolTableEntry(VarOffset(scopeOffset)));

        Storage storage = {
            identifier,
            identifierIndex,
            scopeOffset
        };
        m_storages[index] = storage;
        return storage;
    }

    BytecodeGenerator& m_bytecodeGenerator;
    InstructionStream::Offset m_enterPoint;
    Optional<GeneratorFrameData> m_generatorFrameData;
    UnlinkedCodeBlock* m_codeBlock;
    InstructionStreamWriter& m_instructions;
    BytecodeGraph m_graph;
    Vector<Optional<Storage>> m_storages;
    Yields m_yields;
    Strong<SymbolTable> m_generatorFrameSymbolTable;
    int m_generatorFrameSymbolTableIndex;
};

class GeneratorLivenessAnalysis : public BytecodeLivenessPropagation {
public:
    GeneratorLivenessAnalysis(BytecodeGeneratorification& generatorification)
        : m_generatorification(generatorification)
    {
    }

    void run(UnlinkedCodeBlock* codeBlock, InstructionStreamWriter& instructions)
    {
        // Perform modified liveness analysis to determine which locals are live at the merge points.
        // This produces the conservative results for the question, "which variables should be saved and resumed?".

        runLivenessFixpoint(codeBlock, instructions, m_generatorification.graph());

        for (YieldData& data : m_generatorification.yields())
            data.liveness = getLivenessInfoAtBytecodeOffset(codeBlock, instructions, m_generatorification.graph(), m_generatorification.instructions().at(data.point).next().offset());
    }

private:
    BytecodeGeneratorification& m_generatorification;
};

void BytecodeGeneratorification::run()
{
    // We calculate the liveness at each merge point. This gives us the information which registers should be saved and resumed conservatively.

    VM& vm = m_bytecodeGenerator.vm();
    {
        GeneratorLivenessAnalysis pass(*this);
        pass.run(m_codeBlock, m_instructions);
    }

    BytecodeRewriter rewriter(m_bytecodeGenerator, m_graph, m_codeBlock, m_instructions);

    // Setup the global switch for the generator.
    {
        auto nextToEnterPoint = enterPoint().next();
        unsigned switchTableIndex = m_codeBlock->numberOfSwitchJumpTables();
        VirtualRegister state = virtualRegisterForArgument(static_cast<int32_t>(JSGeneratorFunction::GeneratorArgument::State));
        auto& jumpTable = m_codeBlock->addSwitchJumpTable();
        jumpTable.min = 0;
        jumpTable.branchOffsets.resize(m_yields.size() + 1);
        jumpTable.branchOffsets.fill(0);
        jumpTable.add(0, nextToEnterPoint.offset());
        for (unsigned i = 0; i < m_yields.size(); ++i)
            jumpTable.add(i + 1, m_yields[i].point);

        rewriter.insertFragmentBefore(nextToEnterPoint, [&] (BytecodeRewriter::Fragment& fragment) {
            fragment.appendInstruction<OpSwitchImm>(switchTableIndex, BoundLabel(nextToEnterPoint.offset()), state);
        });
    }

    for (const YieldData& data : m_yields) {
        VirtualRegister scope = virtualRegisterForArgument(static_cast<int32_t>(JSGeneratorFunction::GeneratorArgument::Frame));

        auto instruction = m_instructions.at(data.point);
        // Emit save sequence.
        rewriter.insertFragmentBefore(instruction, [&] (BytecodeRewriter::Fragment& fragment) {
            data.liveness.forEachSetBit([&](size_t index) {
                VirtualRegister operand = virtualRegisterForLocal(index);
                Storage storage = storageForGeneratorLocal(vm, index);

                fragment.appendInstruction<OpPutToScope>(
                    scope, // scope
                    storage.identifierIndex, // identifier
                    operand, // value
                    GetPutInfo(DoNotThrowIfNotFound, LocalClosureVar, InitializationMode::NotInitialization), // info
                    SymbolTableOrScopeDepth::symbolTable(VirtualRegister { m_generatorFrameSymbolTableIndex }), // symbol table constant index
                    storage.scopeOffset.offset() // scope offset
                );
            });

            // Insert op_ret just after save sequence.
            fragment.appendInstruction<OpRet>(data.argument);
        });

        // Emit resume sequence.
        rewriter.insertFragmentAfter(instruction, [&] (BytecodeRewriter::Fragment& fragment) {
            data.liveness.forEachSetBit([&](size_t index) {
                VirtualRegister operand = virtualRegisterForLocal(index);
                Storage storage = storageForGeneratorLocal(vm, index);

                fragment.appendInstruction<OpGetFromScope>(
                    operand, // dst
                    scope, // scope
                    storage.identifierIndex, // identifier
                    GetPutInfo(DoNotThrowIfNotFound, LocalClosureVar, InitializationMode::NotInitialization), // info
                    0, // local scope depth
                    storage.scopeOffset.offset() // scope offset
                );
            });
        });

        // Clip the unnecessary bytecodes.
        rewriter.removeBytecode(instruction);
    }

    if (m_generatorFrameData) {
        auto instruction = m_instructions.at(m_generatorFrameData->m_point);
        rewriter.insertFragmentAfter(instruction, [&] (BytecodeRewriter::Fragment& fragment) {
            if (!m_generatorFrameSymbolTable->scopeSize()) {
                // This will cause us to put jsUndefined() into the generator frame's scope value.
                fragment.appendInstruction<OpMov>(m_generatorFrameData->m_dst, m_generatorFrameData->m_initialValue);
            } else
                fragment.appendInstruction<OpCreateLexicalEnvironment>(m_generatorFrameData->m_dst, m_generatorFrameData->m_scope, m_generatorFrameData->m_symbolTable, m_generatorFrameData->m_initialValue);
        });
        rewriter.removeBytecode(instruction);
    }

    rewriter.execute();
}

void performGeneratorification(BytecodeGenerator& bytecodeGenerator, UnlinkedCodeBlock* codeBlock, InstructionStreamWriter& instructions, SymbolTable* generatorFrameSymbolTable, int generatorFrameSymbolTableIndex)
{
    if (Options::dumpBytecodesBeforeGeneratorification())
        BytecodeDumper<UnlinkedCodeBlock>::dumpBlock(codeBlock, instructions, WTF::dataFile());

    BytecodeGeneratorification pass(bytecodeGenerator, codeBlock, instructions, generatorFrameSymbolTable, generatorFrameSymbolTableIndex);
    pass.run();
}

} // namespace JSC
