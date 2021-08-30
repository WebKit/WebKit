/*
 * Copyright (C) 2011-2021 Apple Inc. All rights reserved.
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

extern "C" JS_EXPORT_PRIVATE Opcode g_opcodeMap[numOpcodeIDs + numWasmOpcodeIDs];
extern "C" JS_EXPORT_PRIVATE Opcode g_opcodeMapWide16[numOpcodeIDs + numWasmOpcodeIDs];
extern "C" JS_EXPORT_PRIVATE Opcode g_opcodeMapWide32[numOpcodeIDs + numWasmOpcodeIDs];

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
    friend const Opcode* getOpcodeAddress(OpcodeID);
    friend const Opcode* getOpcodeWide16Address(OpcodeID);
    friend const Opcode* getOpcodeWide32Address(OpcodeID);
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
    return g_opcodeMap;
}

inline Opcode* opcodeMapWide16()
{
    return g_opcodeMapWide16;
}

inline Opcode* opcodeMapWide32()
{
    return g_opcodeMapWide32;
}

inline Opcode getOpcode(OpcodeID id)
{
#if ENABLE(COMPUTED_GOTO_OPCODES)
    return g_opcodeMap[id];
#else
    return static_cast<Opcode>(id);
#endif
}

inline Opcode getOpcodeWide16(OpcodeID id)
{
#if ENABLE(COMPUTED_GOTO_OPCODES)
    return g_opcodeMapWide16[id];
#else
    UNUSED_PARAM(id);
    RELEASE_ASSERT_NOT_REACHED();
#endif
}

inline Opcode getOpcodeWide32(OpcodeID id)
{
#if ENABLE(COMPUTED_GOTO_OPCODES)
    return g_opcodeMapWide32[id];
#else
    UNUSED_PARAM(id);
    RELEASE_ASSERT_NOT_REACHED();
#endif
}


#if ENABLE(COMPUTED_GOTO_OPCODES)
inline const Opcode* getOpcodeAddress(OpcodeID id)
{
    return &g_opcodeMap[id];
}

inline const Opcode* getOpcodeWide16Address(OpcodeID id)
{
    return &g_opcodeMapWide16[id];
}

inline const Opcode* getOpcodeWide32Address(OpcodeID id)
{
    return &g_opcodeMapWide32[id];
}
#endif

template<PtrTag tag>
ALWAYS_INLINE MacroAssemblerCodePtr<tag> getCodePtrImpl(const Opcode opcode, const void* opcodeAddress)
{
    void* opcodeValue = reinterpret_cast<void*>(opcode);
    void* untaggedOpcode = untagAddressDiversifiedCodePtr<BytecodePtrTag>(opcodeValue, opcodeAddress);
    void* retaggedOpcode = tagCodePtr<tag>(untaggedOpcode);
    return MacroAssemblerCodePtr<tag>::createFromExecutableAddress(retaggedOpcode);
}

#if ENABLE(ARM64E) && !ENABLE(COMPUTED_GOTO_OPCODES)
#error ENABLE(ARM64E) requires ENABLE(COMPUTED_GOTO_OPCODES) for getCodePtr (and its variants).
#endif

template<PtrTag tag>
ALWAYS_INLINE MacroAssemblerCodePtr<tag> getCodePtr(OpcodeID opcodeID)
{
#if ENABLE(COMPUTED_GOTO_OPCODES)
    const Opcode* opcode = getOpcodeAddress(opcodeID);
    return getCodePtrImpl<tag>(*opcode, opcode);
#else
    return getCodePtrImpl<tag>(getOpcode(opcodeID), nullptr);
#endif
}

template<PtrTag tag>
ALWAYS_INLINE MacroAssemblerCodePtr<tag> getWide16CodePtr(OpcodeID opcodeID)
{
#if ENABLE(COMPUTED_GOTO_OPCODES)
    const Opcode* opcode = getOpcodeWide16Address(opcodeID);
    return getCodePtrImpl<tag>(*opcode, opcode);
#else
    return getCodePtrImpl<tag>(getOpcodeWide16(opcodeID), nullptr);
#endif
}

template<PtrTag tag>
ALWAYS_INLINE MacroAssemblerCodePtr<tag> getWide32CodePtr(OpcodeID opcodeID)
{
#if ENABLE(COMPUTED_GOTO_OPCODES)
    const Opcode* opcode = getOpcodeWide32Address(opcodeID);
    return getCodePtrImpl<tag>(*opcode, opcode);
#else
    return getCodePtrImpl<tag>(getOpcodeWide32(opcodeID), nullptr);
#endif
}

template<PtrTag tag>
ALWAYS_INLINE MacroAssemblerCodeRef<tag> getCodeRef(OpcodeID opcodeID)
{
    return MacroAssemblerCodeRef<tag>::createSelfManagedCodeRef(getCodePtr<tag>(opcodeID));
}

template<PtrTag tag>
ALWAYS_INLINE MacroAssemblerCodeRef<tag> getWide16CodeRef(OpcodeID opcodeID)
{
    return MacroAssemblerCodeRef<tag>::createSelfManagedCodeRef(getWide16CodePtr<tag>(opcodeID));
}

template<PtrTag tag>
ALWAYS_INLINE MacroAssemblerCodeRef<tag> getWide32CodeRef(OpcodeID opcodeID)
{
    return MacroAssemblerCodeRef<tag>::createSelfManagedCodeRef(getWide32CodePtr<tag>(opcodeID));
}

#if ENABLE(JIT)
template<PtrTag tag>
ALWAYS_INLINE LLIntCode getCodeFunctionPtr(OpcodeID opcodeID)
{
#if COMPILER(MSVC)
    return reinterpret_cast<LLIntCode>(getCodePtr<tag>(opcodeID).executableAddress());
#else
    return reinterpret_cast<LLIntCode>(getCodePtr<tag>(opcodeID).template executableAddress());
#endif
}

template<PtrTag tag>
ALWAYS_INLINE LLIntCode getWide16CodeFunctionPtr(OpcodeID opcodeID)
{
#if COMPILER(MSVC)
    return reinterpret_cast<LLIntCode>(getWide16CodePtr<tag>(opcodeID).executableAddress());
#else
    return reinterpret_cast<LLIntCode>(getWide16CodePtr<tag>(opcodeID).template executableAddress());
#endif
}

template<PtrTag tag>
ALWAYS_INLINE LLIntCode getWide32CodeFunctionPtr(OpcodeID opcodeID)
{
#if COMPILER(MSVC)
    return reinterpret_cast<LLIntCode>(getWide32CodePtr<tag>(opcodeID).executableAddress());
#else
    return reinterpret_cast<LLIntCode>(getWide32CodePtr<tag>(opcodeID).template executableAddress());
#endif
}

#if ENABLE(WEBASSEMBLY)

inline Opcode getOpcode(WasmOpcodeID id)
{
#if ENABLE(COMPUTED_GOTO_OPCODES)
    return g_opcodeMap[numOpcodeIDs + id];
#else
    return static_cast<Opcode>(id);
#endif
}

inline Opcode getOpcodeWide16(WasmOpcodeID id)
{
#if ENABLE(COMPUTED_GOTO_OPCODES)
    return g_opcodeMapWide16[numOpcodeIDs + id];
#else
    UNUSED_PARAM(id);
    RELEASE_ASSERT_NOT_REACHED();
#endif
}

inline Opcode getOpcodeWide32(WasmOpcodeID id)
{
#if ENABLE(COMPUTED_GOTO_OPCODES)
    return g_opcodeMapWide32[numOpcodeIDs + id];
#else
    UNUSED_PARAM(id);
    RELEASE_ASSERT_NOT_REACHED();
#endif
}

#if ENABLE(COMPUTED_GOTO_OPCODES)
inline const Opcode* getOpcodeAddress(WasmOpcodeID id)
{
    return &g_opcodeMap[numOpcodeIDs + id];
}

inline const Opcode* getOpcodeWide16Address(WasmOpcodeID id)
{
    return &g_opcodeMapWide16[numOpcodeIDs + id];
}

inline const Opcode* getOpcodeWide32Address(WasmOpcodeID id)
{
    return &g_opcodeMapWide32[numOpcodeIDs + id];
}
#endif

template<PtrTag tag>
ALWAYS_INLINE MacroAssemblerCodePtr<tag> getCodePtr(WasmOpcodeID opcodeID)
{
#if ENABLE(COMPUTED_GOTO_OPCODES)
    const Opcode* opcode = getOpcodeAddress(opcodeID);
    return getCodePtrImpl<tag>(*opcode, opcode);
#else
    return getCodePtrImpl<tag>(getOpcode(opcodeID), nullptr);
#endif
}

template<PtrTag tag>
ALWAYS_INLINE MacroAssemblerCodePtr<tag> getWide16CodePtr(WasmOpcodeID opcodeID)
{
#if ENABLE(COMPUTED_GOTO_OPCODES)
    const Opcode* opcode = getOpcodeWide16Address(opcodeID);
    return getCodePtrImpl<tag>(*opcode, opcode);
#else
    return getCodePtrImpl<tag>(getOpcodeWide16(opcodeID), nullptr);
#endif
}

template<PtrTag tag>
ALWAYS_INLINE MacroAssemblerCodePtr<tag> getWide32CodePtr(WasmOpcodeID opcodeID)
{
#if ENABLE(COMPUTED_GOTO_OPCODES)
    const Opcode* opcode = getOpcodeWide32Address(opcodeID);
    return getCodePtrImpl<tag>(*opcode, opcode);
#else
    return getCodePtrImpl<tag>(getOpcodeWide32(opcodeID), nullptr);
#endif
}

template<PtrTag tag>
ALWAYS_INLINE LLIntCode getCodeFunctionPtr(WasmOpcodeID opcodeID)
{
#if COMPILER(MSVC)
    return reinterpret_cast<LLIntCode>(getCodePtr<tag>(opcodeID).executableAddress());
#else
    return reinterpret_cast<LLIntCode>(getCodePtr<tag>(opcodeID).template executableAddress());
#endif
}

template<PtrTag tag>
ALWAYS_INLINE LLIntCode getWide16CodeFunctionPtr(WasmOpcodeID opcodeID)
{
#if COMPILER(MSVC)
    return reinterpret_cast<LLIntCode>(getWide16CodePtr<tag>(opcodeID).executableAddress());
#else
    return reinterpret_cast<LLIntCode>(getWide16CodePtr<tag>(opcodeID).template executableAddress());
#endif
}

template<PtrTag tag>
ALWAYS_INLINE LLIntCode getWide32CodeFunctionPtr(WasmOpcodeID opcodeID)
{
#if COMPILER(MSVC)
    return reinterpret_cast<LLIntCode>(getWide32CodePtr<tag>(opcodeID).executableAddress());
#else
    return reinterpret_cast<LLIntCode>(getWide32CodePtr<tag>(opcodeID).template executableAddress());
#endif
}

#endif // ENABLE(WEBASSEMBLY)

#else // not ENABLE(JIT)
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
#endif // ENABLE(JIT)

#if ENABLE(JIT)
struct Registers {
    static constexpr GPRReg pcGPR = GPRInfo::regT4;

#if CPU(X86_64) && !OS(WINDOWS)
    static constexpr GPRReg metadataTableGPR = GPRInfo::regCS1;
    static constexpr GPRReg pbGPR = GPRInfo::regCS2;
#elif CPU(X86_64) && OS(WINDOWS)
    static constexpr GPRReg metadataTableGPR = GPRInfo::regCS3;
    static constexpr GPRReg pbGPR = GPRInfo::regCS4;
#elif CPU(ARM64) || CPU(RISCV64)
    static constexpr GPRReg metadataTableGPR = GPRInfo::regCS6;
    static constexpr GPRReg pbGPR = GPRInfo::regCS7;
#elif CPU(MIPS) || CPU(ARM_THUMB2)
    static constexpr GPRReg metadataTableGPR = GPRInfo::regCS0;
    static constexpr GPRReg pbGPR = GPRInfo::regCS1;
#endif
};
#endif

} } // namespace JSC::LLInt
