/*
 * Copyright (C) 2017 Yusuke Suzuki <utatane.tea@gmail.com>
 * Copyright (C) 2017-2022 Apple Inc. All rights reserved.
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

#include "BytecodeGenerator.h"
#include "BytecodeGraph.h"
#include "BytecodeStructs.h"
#include "CodeBlock.h"
#include "JSCJSValueInlines.h"
#include "UnlinkedCodeBlockGenerator.h"
#include "UnlinkedMetadataTableInlines.h"
#include "WasmFunctionCodeBlockGenerator.h"
#include "WasmGeneratorTraits.h"
#include "WasmModuleInformation.h"
#include "WasmTypeDefinitionInlines.h"

namespace JSC {

template<typename InstructionStreamType>
void BytecodeDumperBase<InstructionStreamType>::printLocationAndOp(typename InstructionStreamType::Offset location, const char* op)
{
    m_currentLocation = location;
    m_out.printf("[%4u] %-18s ", location, op);
}

template<typename InstructionStreamType>
void BytecodeDumperBase<InstructionStreamType>::dumpValue(VirtualRegister reg)
{
    m_out.printf("%s", registerName(reg).data());
}

template<typename InstructionStreamType>
template<typename Traits>
void BytecodeDumperBase<InstructionStreamType>::dumpValue(GenericBoundLabel<Traits> label)
{
    int target = label.target();
    if (!target)
        target = outOfLineJumpOffset(m_currentLocation);
    auto targetOffset = target + m_currentLocation;
    m_out.print(target, "(->", targetOffset, ")");
}

template void BytecodeDumperBase<JSInstructionStream>::dumpValue(GenericBoundLabel<JSGeneratorTraits>);

#if ENABLE(WEBASSEMBLY)
template void BytecodeDumperBase<WasmInstructionStream>::dumpValue(GenericBoundLabel<Wasm::GeneratorTraits>);
#endif // ENABLE(WEBASSEMBLY)

template<class Block>
CString BytecodeDumper<Block>::registerName(VirtualRegister r) const
{
    if (r.isConstant())
        return constantName(r);

    return toCString(r);
}

template <class Block>
int BytecodeDumper<Block>::outOfLineJumpOffset(JSInstructionStream::Offset offset) const
{
    return m_block->outOfLineJumpOffset(offset);
}

template<class Block>
CString BytecodeDumper<Block>::constantName(VirtualRegister reg) const
{
    if (reg.toConstantIndex() >= (int) block()->constantRegisters().size())
        return toCString("INVALID_CONSTANT(", reg, ")");
    auto value = block()->getConstant(reg);
    return toCString(value, "(", reg, ")");
}

template<class Block>
void BytecodeDumper<Block>::dumpBytecode(const JSInstructionStream::Ref& it, const ICStatusMap&)
{
    ::JSC::dumpBytecode(this, it.offset(), it.ptr());
    this->m_out.print("\n");
}

template<class Block>
void BytecodeDumper<Block>::dumpBytecode(Block* block, PrintStream& out, const JSInstructionStream::Ref& it, const ICStatusMap& statusMap)
{
    BytecodeDumper dumper(block, out);
    dumper.dumpBytecode(it, statusMap);
}

template<class Block>
VM& CodeBlockBytecodeDumper<Block>::vm() const
{
    return this->block()->vm();
}

template<class Block>
const Identifier& CodeBlockBytecodeDumper<Block>::identifier(int index) const
{
    return this->block()->identifier(index);
}

template<class Block>
void CodeBlockBytecodeDumper<Block>::dumpIdentifiers()
{
    if (size_t count = this->block()->numberOfIdentifiers()) {
        this->m_out.printf("\nIdentifiers:\n");
        size_t i = 0;
        do {
            this->m_out.print("  id", static_cast<unsigned>(i), " = ", identifier(i), "\n");
            ++i;
        } while (i != count);
    }
}

template<class Block>
void CodeBlockBytecodeDumper<Block>::dumpConstants()
{
    if (!this->block()->constantRegisters().isEmpty()) {
        this->m_out.printf("\nConstants:\n");
        size_t i = 0;
        for (const auto& constant : this->block()->constantRegisters()) {
            const char* sourceCodeRepresentationDescription = nullptr;
            switch (this->block()->constantSourceCodeRepresentation(i)) {
            case SourceCodeRepresentation::Double:
                sourceCodeRepresentationDescription = ": in source as double";
                break;
            case SourceCodeRepresentation::Integer:
                sourceCodeRepresentationDescription = ": in source as integer";
                break;
            case SourceCodeRepresentation::Other:
                sourceCodeRepresentationDescription = "";
                break;
            case SourceCodeRepresentation::LinkTimeConstant:
                sourceCodeRepresentationDescription = ": in source as link-time-constant";
                break;
            }
            this->m_out.printf("   k%u = %s%s\n", static_cast<unsigned>(i), toCString(constant.get()).data(), sourceCodeRepresentationDescription);
            ++i;
        }
    }
}

template<class Block>
void CodeBlockBytecodeDumper<Block>::dumpExceptionHandlers()
{
    if (unsigned count = this->block()->numberOfExceptionHandlers()) {
        this->m_out.printf("\nException Handlers:\n");
        unsigned i = 0;
        do {
            const auto& handler = this->block()->exceptionHandler(i);
            this->m_out.printf("\t %d: { start: [%4d] end: [%4d] target: [%4d] } %s\n", i + 1, handler.start, handler.end, handler.target, handler.typeName());
            ++i;
        } while (i < count);
    }
}

template<class Block>
void CodeBlockBytecodeDumper<Block>::dumpSwitchJumpTables()
{
    if (unsigned count = this->block()->numberOfUnlinkedSwitchJumpTables()) {
        this->m_out.printf("Switch Jump Tables:\n");
        unsigned i = 0;
        do {
            this->m_out.printf("  %1d = {\n", i);
            const auto& switchJumpTable = this->block()->unlinkedSwitchJumpTable(i);
            int entry = 0;
            auto end = switchJumpTable.m_branchOffsets.end();
            for (auto iter = switchJumpTable.m_branchOffsets.begin(); iter != end; ++iter, ++entry) {
                if (!*iter)
                    continue;
                this->m_out.printf("\t\t%4d => %04d\n", entry + switchJumpTable.m_min, *iter);
            }
            this->m_out.printf("      }\n");
            ++i;
        } while (i < count);
    }
}

template<class Block>
void CodeBlockBytecodeDumper<Block>::dumpStringSwitchJumpTables()
{
    if (unsigned count = this->block()->numberOfUnlinkedStringSwitchJumpTables()) {
        this->m_out.printf("\nString Switch Jump Tables:\n");
        unsigned i = 0;
        do {
            this->m_out.printf("  %1d = {\n", i);
            for (const auto& entry : this->block()->unlinkedStringSwitchJumpTable(i).m_offsetTable)
                this->m_out.printf("\t\t\"%s\" => %04d\n", entry.key->utf8().data(), entry.value.m_branchOffset);
            this->m_out.printf("      }\n");
            ++i;
        } while (i < count);
    }
}

template <typename Block>
static void dumpHeader(Block* block, const JSInstructionStream& instructions, PrintStream& out)
{
    size_t instructionCount = 0;
    size_t wide16InstructionCount = 0;
    size_t wide32InstructionCount = 0;
    size_t instructionWithMetadataCount = 0;

    for (const auto& instruction : instructions) {
        if (instruction->isWide16())
            ++wide16InstructionCount;
        else if (instruction->isWide32())
            ++wide32InstructionCount;
        if (instruction->hasMetadata())
            ++instructionWithMetadataCount;
        ++instructionCount;
    }

    out.print(*block);
    out.printf(
        ": %lu instructions (%lu 16-bit instructions, %lu 32-bit instructions, %lu instructions with metadata); %lu bytes (%lu metadata bytes); %d parameter(s); %d callee register(s); %d variable(s)",
        static_cast<unsigned long>(instructionCount),
        static_cast<unsigned long>(wide16InstructionCount),
        static_cast<unsigned long>(wide32InstructionCount),
        static_cast<unsigned long>(instructionWithMetadataCount),
        static_cast<unsigned long>(instructions.sizeInBytes() + block->metadataSizeInBytes()),
        static_cast<unsigned long>(block->metadataSizeInBytes()),
        block->numParameters(), block->numCalleeLocals(), block->numVars());
    out.print("; scope at ", block->scopeRegister());
    out.printf("\n");
}

template <typename Dumper>
static void dumpFooter(Dumper& dumper)
{
    dumper.dumpIdentifiers();
    dumper.dumpConstants();
    dumper.dumpExceptionHandlers();
    dumper.dumpSwitchJumpTables();
    dumper.dumpStringSwitchJumpTables();
}

template<class Block>
void CodeBlockBytecodeDumper<Block>::dumpBlock(Block* block, const JSInstructionStream& instructions, PrintStream& out, const ICStatusMap& statusMap)
{
    dumpHeader(block, instructions, out);

    CodeBlockBytecodeDumper<Block> dumper(block, out);
    for (const auto& it : instructions)
        dumper.dumpBytecode(it, statusMap);

    dumpFooter(dumper);

    out.printf("\n");
}

template<class Block>
void CodeBlockBytecodeDumper<Block>::dumpGraph(Block* block, const JSInstructionStream& instructions, BytecodeGraph& graph, PrintStream& out, const ICStatusMap& icStatusMap)
{
    dumpHeader(block, instructions, out);

    CodeBlockBytecodeDumper<Block> dumper(block, out);

    out.printf("\n");

    Vector<Vector<unsigned>> predecessors;
    predecessors.resize(graph.size());
    for (auto& block : graph) {
        if (block.isEntryBlock() || block.isExitBlock())
            continue;
        for (auto successorIndex : block.successors()) {
            if (!predecessors[successorIndex].contains(block.index()))
                predecessors[successorIndex].append(block.index());
        }
    }

    for (auto& block : graph) {
        if (block.isEntryBlock() || block.isExitBlock())
            continue;

        out.print("bb#", block.index(), "\n");

        out.print("Predecessors: [");
        for (unsigned predecessor : predecessors[block.index()]) {
            if (!graph[predecessor].isEntryBlock())
                out.print(" #", predecessor);
        }
        out.print(" ]\n");

        for (unsigned i = 0; i < block.totalLength(); ) {
            auto& currentInstruction = instructions.at(i + block.leaderOffset());
            dumper.dumpBytecode(currentInstruction, icStatusMap);
            i += currentInstruction.ptr()->size();
        }

        out.print("Successors: [");
        for (unsigned successor : block.successors()) {
            if (!graph[successor].isExitBlock())
                out.print(" #", successor);
        }
        out.print(" ]\n\n");
    }

    dumpFooter(dumper);

    out.printf("\n");
}

template class BytecodeDumperBase<JSInstructionStream>;
template class BytecodeDumper<CodeBlock>;
template class CodeBlockBytecodeDumper<UnlinkedCodeBlockGenerator>;
template class CodeBlockBytecodeDumper<CodeBlock>;

#if ENABLE(WEBASSEMBLY)

template class BytecodeDumperBase<WasmInstructionStream>;

namespace Wasm {

void BytecodeDumper::dumpBlock(FunctionCodeBlockGenerator* block, const ModuleInformation& moduleInformation, PrintStream& out)
{
    size_t instructionCount = 0;
    size_t wide16InstructionCount = 0;
    size_t wide32InstructionCount = 0;

    for (auto it = block->instructions().begin(); it != block->instructions().end(); it += it->size()) {
        if (it->isWide16())
            ++wide16InstructionCount;
        else if (it->isWide32())
            ++wide32InstructionCount;
        ++instructionCount;
    }

    size_t functionIndexSpace = moduleInformation.importFunctionCount() + block->functionIndex();
    out.print(makeString(IndexOrName(functionIndexSpace, moduleInformation.nameSection->get(functionIndexSpace))));

    const auto& function = moduleInformation.functions[block->functionIndex()];
    TypeIndex typeIndex = moduleInformation.internalFunctionTypeIndices[block->functionIndex()];
    const auto& typeDefinition = TypeInformation::get(typeIndex);
    out.print(" : ", typeDefinition, "\n");
    out.print("wasm size: ", function.data.size(), " bytes\n");

    out.printf(
        "bytecode: %lu instructions (%lu 16-bit instructions, %lu 32-bit instructions); %lu bytes; %d parameter(s); %d local(s); %d callee register(s)\n",
        static_cast<unsigned long>(instructionCount),
        static_cast<unsigned long>(wide16InstructionCount),
        static_cast<unsigned long>(wide32InstructionCount),
        static_cast<unsigned long>(block->instructions().sizeInBytes()),
        block->numArguments(),
        block->numVars(),
        block->numCalleeLocals());

    BytecodeDumper dumper(block, out);
    for (auto it = block->instructions().begin(); it != block->instructions().end(); it += it->size()) {
        dumpWasm(&dumper, it.offset(), it.ptr());
        out.print("\n");
    }

    dumper.dumpConstants();
    dumper.dumpExceptionHandlers();

    out.printf("\n");
}

void BytecodeDumper::dumpConstants()
{
    FunctionCodeBlockGenerator* block = this->block();
    if (!block->constants().isEmpty()) {
        this->m_out.printf("\nConstants:\n");
        unsigned i = 0;
        for (const auto& constant : block->constants()) {
            Type type = block->constantTypes()[i];
            this->m_out.print("   const", i, " : ", type.kind, " = ", formatConstant(type, constant), "\n");
            ++i;
        }
    }
}

void BytecodeDumper::dumpExceptionHandlers()
{
    if (unsigned count = this->block()->numberOfExceptionHandlers()) {
        this->m_out.printf("\nException Handlers:\n");
        unsigned i = 0;
        do {
            const auto& handler = this->block()->exceptionHandler(i);
            this->m_out.printf("\t %d: { start: [%4d] end: [%4d] target: [%4d] tryDepth: [%4d] exceptionIndexOrDelegateTarget: [%4d] } %s\n", i + 1, handler.m_start, handler.m_end, handler.m_target, handler.m_tryDepth, handler.m_exceptionIndexOrDelegateTarget, handler.typeName().characters8());
            ++i;
        } while (i < count);
    }
}

CString BytecodeDumper::constantName(VirtualRegister index) const
{
    FunctionCodeBlockGenerator* block = this->block();
    auto value = formatConstant(block->getConstantType(index), block->getConstant(index));
    return toCString(value, "(", VirtualRegister(index), ")");
}

CString BytecodeDumper::formatConstant(Type type, uint64_t constant) const
{
    switch (type.kind) {
    case TypeKind::I32:
        return toCString(static_cast<int32_t>(constant));
    case TypeKind::I64:
        return toCString(constant);
    case TypeKind::F32:
        return toCString(bitwise_cast<float>(static_cast<int32_t>(constant)));
        break;
    case TypeKind::F64:
        return toCString(bitwise_cast<double>(constant));
        break;
    case TypeKind::V128:
        return toCString(constant);
        break;
    default: {
        if (isFuncref(type) || isExternref(type)) {
            if (JSValue::decode(constant) == jsNull())
                return "null";
            return toCString(RawHex(constant));
        }

        RELEASE_ASSERT_NOT_REACHED();
        return "";
    }
    }
}

CString BytecodeDumper::registerName(VirtualRegister r) const
{
    if (r.isConstant())
        return constantName(r);

    return toCString(r);
}

int BytecodeDumper::outOfLineJumpOffset(WasmInstructionStream::Offset offset) const
{
    return m_block->outOfLineJumpOffset(offset);
}

} // namespace Wasm

#endif // ENABLE(WEBASSEMBLY)
}
