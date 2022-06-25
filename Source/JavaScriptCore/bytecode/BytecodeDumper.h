/*
 * Copyright (C) 2017 Yusuke Suzuki <utatane.tea@gmail.com>
 * Copyright (C) 2018-2021 Apple Inc. All rights reserved.
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
#include "BytecodeGeneratorBase.h"
#include "CallLinkInfo.h"
#include "ICStatusMap.h"
#include "Instruction.h"
#include "InstructionStream.h"
#include "StructureStubInfo.h"
#include "WasmOps.h"

namespace JSC {

class BytecodeGraph;

template<typename InstructionStreamType>
class BytecodeDumperBase {
public:
    virtual ~BytecodeDumperBase()
    {
    }

    void printLocationAndOp(typename InstructionStreamType::Offset location, const char* op);

    template<typename T>
    void dumpOperand(const char* operandName, T operand, bool isFirst = false)
    {
        if (!isFirst)
            m_out.print(", ");
        m_out.print(operandName);
        m_out.print(":");
        dumpValue(operand);
    }

    void dumpValue(VirtualRegister);

    template<typename Traits>
    void dumpValue(GenericBoundLabel<Traits>);

    template<typename T>
    void dumpValue(T v) { m_out.print(v); }

protected:
    virtual CString registerName(VirtualRegister) const = 0;
    virtual int outOfLineJumpOffset(typename InstructionStreamType::Offset) const = 0;

    BytecodeDumperBase(PrintStream& out)
        : m_out(out)
    {
    }

    PrintStream& m_out;
    typename InstructionStreamType::Offset m_currentLocation { 0 };
};

template<class Block>
class BytecodeDumper : public BytecodeDumperBase<JSInstructionStream> {
public:
    static void dumpBytecode(Block*, PrintStream& out, const JSInstructionStream::Ref& it, const ICStatusMap& = ICStatusMap());

    BytecodeDumper(Block* block, PrintStream& out)
        : BytecodeDumperBase(out)
        , m_block(block)
    {
    }

    ~BytecodeDumper() override { }

protected:
    Block* block() const { return m_block; }

    void dumpBytecode(const JSInstructionStream::Ref& it, const ICStatusMap&);

    CString registerName(VirtualRegister) const override;
    int outOfLineJumpOffset(JSInstructionStream::Offset) const override;

private:
    virtual CString constantName(VirtualRegister) const;

    Block* m_block;
};

template<class Block>
class CodeBlockBytecodeDumper final : public BytecodeDumper<Block> {
public:
    static void dumpBlock(Block*, const JSInstructionStream&, PrintStream& out, const ICStatusMap& = ICStatusMap());
    static void dumpGraph(Block*, const JSInstructionStream&, BytecodeGraph&, PrintStream& out = WTF::dataFile(), const ICStatusMap& = ICStatusMap());

    void dumpIdentifiers();
    void dumpConstants();
    void dumpExceptionHandlers();
    void dumpSwitchJumpTables();
    void dumpStringSwitchJumpTables();

private:
    using BytecodeDumper<Block>::BytecodeDumper;

    ALWAYS_INLINE VM& vm() const;

    const Identifier& identifier(int index) const;
};

#if ENABLE(WEBASSEMBLY)

namespace Wasm {

class FunctionCodeBlockGenerator;
struct ModuleInformation;

class BytecodeDumper final : public JSC::BytecodeDumperBase<WasmInstructionStream> {
public:
    static void dumpBlock(FunctionCodeBlockGenerator*, const ModuleInformation&, PrintStream& out);

    BytecodeDumper(FunctionCodeBlockGenerator* block, PrintStream& out)
        : BytecodeDumperBase(out)
        , m_block(block)
    {
    }

    ~BytecodeDumper() override { }

    FunctionCodeBlockGenerator* block() const { return m_block; }

    CString registerName(VirtualRegister) const override;
    int outOfLineJumpOffset(WasmInstructionStream::Offset) const override;

private:
    void dumpConstants();
    void dumpExceptionHandlers();
    CString constantName(VirtualRegister index) const;
    CString formatConstant(Type, uint64_t) const;

    FunctionCodeBlockGenerator* m_block;
};

} // namespace Wasm

#endif // ENABLE(WEBASSEMBLY)

}
