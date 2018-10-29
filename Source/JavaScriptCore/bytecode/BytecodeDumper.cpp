/*
 * Copyright (C) 2017 Yusuke Suzuki <utatane.tea@gmail.com>
 * Copyright (C) 2017-2018 Apple Inc. All rights reserved.
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
#include "BytecodeDumper.h"

#include "ArithProfile.h"
#include "BytecodeStructs.h"
#include "CallLinkStatus.h"
#include "CodeBlock.h"
#include "Error.h"
#include "HeapInlines.h"
#include "InterpreterInlines.h"
#include "PolymorphicAccess.h"
#include "PutByIdFlags.h"
#include "StructureInlines.h"
#include "ToThisStatus.h"
#include "UnlinkedCodeBlock.h"
#include "UnlinkedMetadataTableInlines.h"

namespace JSC {

template<class Block>
VM* BytecodeDumper<Block>::vm() const
{
    return block()->vm();
}

template<class Block>
const Identifier& BytecodeDumper<Block>::identifier(int index) const
{
    return block()->identifier(index);
}

static ALWAYS_INLINE bool isConstantRegisterIndex(int index)
{
    return index >= FirstConstantRegisterIndex;
}

template<class Block>
CString BytecodeDumper<Block>::registerName(int r) const
{
    if (isConstantRegisterIndex(r))
        return constantName(r);

    return toCString(VirtualRegister(r));
}

template<class Block>
CString BytecodeDumper<Block>::constantName(int index) const
{
    JSValue value = block()->getConstant(index);
    return toCString(value, "(", VirtualRegister(index), ")");
}

template<class Block>
void BytecodeDumper<Block>::printLocationAndOp(InstructionStream::Offset location, const char* op)
{
    m_out.printf("[%4u] %-18s ", location, op);
}

template<class Block>
void BytecodeDumper<Block>::dumpBytecode(const InstructionStream::Ref& it, const ICStatusMap&)
{
    ::JSC::dumpBytecode(this, it.offset(), it.ptr());
    m_out.print("\n");
}

template<class Block>
void BytecodeDumper<Block>::dumpBytecode(Block* block, PrintStream& out, const InstructionStream::Ref& it, const ICStatusMap& statusMap)
{
    BytecodeDumper dumper(block, out);
    dumper.dumpBytecode(it, statusMap);
}

template<class Block>
void BytecodeDumper<Block>::dumpIdentifiers()
{
    if (size_t count = block()->numberOfIdentifiers()) {
        m_out.printf("\nIdentifiers:\n");
        size_t i = 0;
        do {
            m_out.printf("  id%u = %s\n", static_cast<unsigned>(i), identifier(i).string().utf8().data());
            ++i;
        } while (i != count);
    }
}

template<class Block>
void BytecodeDumper<Block>::dumpConstants()
{
    if (!block()->constantRegisters().isEmpty()) {
        m_out.printf("\nConstants:\n");
        size_t i = 0;
        for (const auto& constant : block()->constantRegisters()) {
            const char* sourceCodeRepresentationDescription = nullptr;
            switch (block()->constantsSourceCodeRepresentation()[i]) {
            case SourceCodeRepresentation::Double:
                sourceCodeRepresentationDescription = ": in source as double";
                break;
            case SourceCodeRepresentation::Integer:
                sourceCodeRepresentationDescription = ": in source as integer";
                break;
            case SourceCodeRepresentation::Other:
                sourceCodeRepresentationDescription = "";
                break;
            }
            m_out.printf("   k%u = %s%s\n", static_cast<unsigned>(i), toCString(constant.get()).data(), sourceCodeRepresentationDescription);
            ++i;
        }
    }
}

template<class Block>
void BytecodeDumper<Block>::dumpExceptionHandlers()
{
    if (unsigned count = block()->numberOfExceptionHandlers()) {
        m_out.printf("\nException Handlers:\n");
        unsigned i = 0;
        do {
            const auto& handler = block()->exceptionHandler(i);
            m_out.printf("\t %d: { start: [%4d] end: [%4d] target: [%4d] } %s\n", i + 1, handler.start, handler.end, handler.target, handler.typeName());
            ++i;
        } while (i < count);
    }
}

template<class Block>
void BytecodeDumper<Block>::dumpSwitchJumpTables()
{
    if (unsigned count = block()->numberOfSwitchJumpTables()) {
        m_out.printf("Switch Jump Tables:\n");
        unsigned i = 0;
        do {
            m_out.printf("  %1d = {\n", i);
            const auto& switchJumpTable = block()->switchJumpTable(i);
            int entry = 0;
            auto end = switchJumpTable.branchOffsets.end();
            for (auto iter = switchJumpTable.branchOffsets.begin(); iter != end; ++iter, ++entry) {
                if (!*iter)
                    continue;
                m_out.printf("\t\t%4d => %04d\n", entry + switchJumpTable.min, *iter);
            }
            m_out.printf("      }\n");
            ++i;
        } while (i < count);
    }
}

template<class Block>
void BytecodeDumper<Block>::dumpStringSwitchJumpTables()
{
    if (unsigned count = block()->numberOfStringSwitchJumpTables()) {
        m_out.printf("\nString Switch Jump Tables:\n");
        unsigned i = 0;
        do {
            m_out.printf("  %1d = {\n", i);
            const auto& stringSwitchJumpTable = block()->stringSwitchJumpTable(i);
            auto end = stringSwitchJumpTable.offsetTable.end();
            for (auto iter = stringSwitchJumpTable.offsetTable.begin(); iter != end; ++iter)
                m_out.printf("\t\t\"%s\" => %04d\n", iter->key->utf8().data(), iter->value.branchOffset);
            m_out.printf("      }\n");
            ++i;
        } while (i < count);
    }
}

template<class Block>
void BytecodeDumper<Block>::dumpBlock(Block* block, const InstructionStream& instructions, PrintStream& out, const ICStatusMap& statusMap)
{
    size_t instructionCount = 0;
    size_t wideInstructionCount = 0;
    size_t instructionWithMetadataCount = 0;

    for (const auto& instruction : instructions) {
        if (instruction->isWide())
            ++wideInstructionCount;
        if (instruction->opcodeID() < NUMBER_OF_BYTECODE_WITH_METADATA)
            ++instructionWithMetadataCount;
        ++instructionCount;
    }

    out.print(*block);
    out.printf(
        ": %lu instructions (%lu wide instructions, %lu instructions with metadata); %lu bytes (%lu metadata bytes); %d parameter(s); %d callee register(s); %d variable(s)",
        static_cast<unsigned long>(instructionCount),
        static_cast<unsigned long>(wideInstructionCount),
        static_cast<unsigned long>(instructionWithMetadataCount),
        static_cast<unsigned long>(instructions.sizeInBytes() + block->metadataSizeInBytes()),
        static_cast<unsigned long>(block->metadataSizeInBytes()),
        block->numParameters(), block->numCalleeLocals(), block->numVars());
    out.print("; scope at ", block->scopeRegister());
    out.printf("\n");

    BytecodeDumper<Block> dumper(block, out);
    for (const auto& it : instructions)
        dumper.dumpBytecode(it, statusMap);

    dumper.dumpIdentifiers();
    dumper.dumpConstants();
    dumper.dumpExceptionHandlers();
    dumper.dumpSwitchJumpTables();
    dumper.dumpStringSwitchJumpTables();

    out.printf("\n");
}

template class BytecodeDumper<UnlinkedCodeBlock>;
template class BytecodeDumper<CodeBlock>;

}
