/*
 * Copyright (C) 2011-2020 Apple Inc. All rights reserved.
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

#pragma once

#include "GPRInfo.h"
#include "Instruction.h"
#include "JSCJSValue.h"
#include "MacroAssemblerCodeRef.h"
#include "Opcode.h"

namespace JSC {

class VM;

#if ENABLE(C_LOOP)
typedef OpcodeID LLIntCode;
#else
typedef void (*LLIntCode)();
#endif

namespace LLInt {

class Data {

public:
    static void performAssertions(VM&);

private:
    friend void initialize();

    friend Instruction* exceptionInstructions();
    friend Instruction* wasmExceptionInstructions();
    friend Opcode* opcodeMap();
    friend Opcode* opcodeMapWide16();
    friend Opcode* opcodeMapWide32();
    friend Opcode getOpcode(OpcodeID);
    friend Opcode getOpcodeWide16(OpcodeID);
    friend Opcode getOpcodeWide32(OpcodeID);
    template<PtrTag tag> friend MacroAssemblerCodePtr<tag> getCodePtr(OpcodeID);
    template<PtrTag tag> friend MacroAssemblerCodePtr<tag> getWide16CodePtr(OpcodeID);
    template<PtrTag tag> friend MacroAssemblerCodePtr<tag> getWide32CodePtr(OpcodeID);
    template<PtrTag tag> friend MacroAssemblerCodeRef<tag> getCodeRef(OpcodeID);
};

void initialize();

inline Instruction* exceptionInstructions()
{
    return reinterpret_cast<Instruction*>(g_jscConfig.llint.exceptionInstructions);
}
    
inline Instruction* wasmExceptionInstructions()
{
    return bitwise_cast<Instruction*>(g_jscConfig.llint.wasmExceptionInstructions);
}

inline Opcode* opcodeMap()
{
    return g_jscConfig.llint.opcodeMap;
}

inline Opcode* opcodeMapWide16()
{
    return g_jscConfig.llint.opcodeMapWide16;
}

inline Opcode* opcodeMapWide32()
{
    return g_jscConfig.llint.opcodeMapWide32;
}

inline Opcode getOpcode(OpcodeID id)
{
#if ENABLE(COMPUTED_GOTO_OPCODES)
    return g_jscConfig.llint.opcodeMap[id];
#else
    return static_cast<Opcode>(id);
#endif
}

inline Opcode getOpcodeWide16(OpcodeID id)
{
#if ENABLE(COMPUTED_GOTO_OPCODES)
    return g_jscConfig.llint.opcodeMapWide16[id];
#else
    UNUSED_PARAM(id);
    RELEASE_ASSERT_NOT_REACHED();
#endif
}

inline Opcode getOpcodeWide32(OpcodeID id)
{
#if ENABLE(COMPUTED_GOTO_OPCODES)
    return g_jscConfig.llint.opcodeMapWide32[id];
#else
    UNUSED_PARAM(id);
    RELEASE_ASSERT_NOT_REACHED();
#endif
}

template<PtrTag tag>
ALWAYS_INLINE MacroAssemblerCodePtr<tag> getCodePtr(OpcodeID opcodeID)
{
    void* address = reinterpret_cast<void*>(getOpcode(opcodeID));
    address = retagCodePtr<BytecodePtrTag, tag>(address);
    return MacroAssemblerCodePtr<tag>::createFromExecutableAddress(address);
}

template<PtrTag tag>
ALWAYS_INLINE MacroAssemblerCodePtr<tag> getWide16CodePtr(OpcodeID opcodeID)
{
    void* address = reinterpret_cast<void*>(getOpcodeWide16(opcodeID));
    address = retagCodePtr<BytecodePtrTag, tag>(address);
    return MacroAssemblerCodePtr<tag>::createFromExecutableAddress(address);
}

template<PtrTag tag>
ALWAYS_INLINE MacroAssemblerCodePtr<tag> getWide32CodePtr(OpcodeID opcodeID)
{
    void* address = reinterpret_cast<void*>(getOpcodeWide32(opcodeID));
    address = retagCodePtr<BytecodePtrTag, tag>(address);
    return MacroAssemblerCodePtr<tag>::createFromExecutableAddress(address);
}

template<PtrTag tag>
ALWAYS_INLINE MacroAssemblerCodePtr<tag> getCodePtr(const Instruction& instruction)
{
    if (instruction.isWide16())
        return getWide16CodePtr<tag>(instruction.opcodeID());
    if (instruction.isWide32())
        return getWide32CodePtr<tag>(instruction.opcodeID());
    return getCodePtr<tag>(instruction.opcodeID());
}

template<PtrTag tag>
ALWAYS_INLINE MacroAssemblerCodeRef<tag> getCodeRef(OpcodeID opcodeID)
{
    return MacroAssemblerCodeRef<tag>::createSelfManagedCodeRef(getCodePtr<tag>(opcodeID));
}

#if ENABLE(JIT)
template<PtrTag tag>
ALWAYS_INLINE LLIntCode getCodeFunctionPtr(OpcodeID opcodeID)
{
    ASSERT(opcodeID >= NUMBER_OF_BYTECODE_IDS);
#if COMPILER(MSVC)
    return reinterpret_cast<LLIntCode>(getCodePtr<tag>(opcodeID).executableAddress());
#else
    return reinterpret_cast<LLIntCode>(getCodePtr<tag>(opcodeID).template executableAddress());
#endif
}

#else
ALWAYS_INLINE void* getCodePtr(OpcodeID id)
{
    return reinterpret_cast<void*>(getOpcode(id));
}

ALWAYS_INLINE void* getWide16CodePtr(OpcodeID id)
{
    return reinterpret_cast<void*>(getOpcodeWide16(id));
}

ALWAYS_INLINE void* getWide32CodePtr(OpcodeID id)
{
    return reinterpret_cast<void*>(getOpcodeWide32(id));
}
#endif

ALWAYS_INLINE void* getCodePtr(JSC::EncodedJSValue glueHelper())
{
    return bitwise_cast<void*>(glueHelper);
}

#if ENABLE(JIT)
struct Registers {
    static constexpr GPRReg pcGPR = GPRInfo::regT4;

#if CPU(X86_64) && !OS(WINDOWS)
    static constexpr GPRReg metadataTableGPR = GPRInfo::regCS1;
    static constexpr GPRReg pbGPR = GPRInfo::regCS2;
#elif CPU(X86_64) && OS(WINDOWS)
    static constexpr GPRReg metadataTableGPR = GPRInfo::regCS3;
    static constexpr GPRReg pbGPR = GPRInfo::regCS4;
#elif CPU(ARM64)
    static constexpr GPRReg metadataTableGPR = GPRInfo::regCS6;
    static constexpr GPRReg pbGPR = GPRInfo::regCS7;
#elif CPU(MIPS) || CPU(ARM_THUMB2)
    static constexpr GPRReg metadataTableGPR = GPRInfo::regCS0;
    static constexpr GPRReg pbGPR = GPRInfo::regCS1;
#endif
};
#endif

} } // namespace JSC::LLInt
