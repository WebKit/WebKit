/*
 * Copyright (C) 2011-2024 Apple Inc. All rights reserved.
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

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

class VM;

#if ENABLE(C_LOOP)
typedef OpcodeID LLIntCode;
#else
typedef void (SYSV_ABI *LLIntCode)();
#endif

namespace LLInt {

extern "C" JS_EXPORT_PRIVATE JSC::Opcode g_opcodeMap[numOpcodeIDs + numWasmOpcodeIDs];
extern "C" JS_EXPORT_PRIVATE JSC::Opcode g_opcodeMapWide16[numOpcodeIDs + numWasmOpcodeIDs];
extern "C" JS_EXPORT_PRIVATE JSC::Opcode g_opcodeMapWide32[numOpcodeIDs + numWasmOpcodeIDs];

class Data {
    friend void initialize();

    friend JSInstruction* exceptionInstructions();
    friend WasmInstruction* wasmExceptionInstructions();
    friend JSC::Opcode* opcodeMap();
    friend JSC::Opcode* opcodeMapWide16();
    friend JSC::Opcode* opcodeMapWide32();
    friend JSC::Opcode getOpcode(OpcodeID);
    friend JSC::Opcode getOpcodeWide16(OpcodeID);
    friend JSC::Opcode getOpcodeWide32(OpcodeID);
    friend const JSC::Opcode* getOpcodeAddress(OpcodeID);
    friend const JSC::Opcode* getOpcodeWide16Address(OpcodeID);
    friend const JSC::Opcode* getOpcodeWide32Address(OpcodeID);
    template<PtrTag tag> friend CodePtr<tag> getCodePtr(OpcodeID);
    template<PtrTag tag> friend CodePtr<tag> getWide16CodePtr(OpcodeID);
    template<PtrTag tag> friend CodePtr<tag> getWide32CodePtr(OpcodeID);
    template<PtrTag tag> friend MacroAssemblerCodeRef<tag> getCodeRef(OpcodeID);
};

void initialize();

inline JSInstruction* exceptionInstructions()
{
    return reinterpret_cast<JSInstruction*>(g_jscConfig.llint.exceptionInstructions);
}

inline WasmInstruction* wasmExceptionInstructions()
{
    return reinterpret_cast<WasmInstruction*>(g_jscConfig.llint.wasmExceptionInstructions);
}

inline JSC::Opcode* opcodeMap()
{
    return g_opcodeMap;
}

inline JSC::Opcode* opcodeMapWide16()
{
    return g_opcodeMapWide16;
}

inline JSC::Opcode* opcodeMapWide32()
{
    return g_opcodeMapWide32;
}

inline JSC::Opcode getOpcode(OpcodeID id)
{
#if ENABLE(COMPUTED_GOTO_OPCODES)
    return g_opcodeMap[id];
#else
    return static_cast<Opcode>(id);
#endif
}

inline JSC::Opcode getOpcodeWide16(OpcodeID id)
{
#if ENABLE(COMPUTED_GOTO_OPCODES)
    return g_opcodeMapWide16[id];
#else
    UNUSED_PARAM(id);
    RELEASE_ASSERT_NOT_REACHED();
#endif
}

inline JSC::Opcode getOpcodeWide32(OpcodeID id)
{
#if ENABLE(COMPUTED_GOTO_OPCODES)
    return g_opcodeMapWide32[id];
#else
    UNUSED_PARAM(id);
    RELEASE_ASSERT_NOT_REACHED();
#endif
}


#if ENABLE(COMPUTED_GOTO_OPCODES)
inline const JSC::Opcode* getOpcodeAddress(OpcodeID id)
{
    return &g_opcodeMap[id];
}

inline const JSC::Opcode* getOpcodeWide16Address(OpcodeID id)
{
    return &g_opcodeMapWide16[id];
}

inline const JSC::Opcode* getOpcodeWide32Address(OpcodeID id)
{
    return &g_opcodeMapWide32[id];
}
#endif

template<PtrTag tag>
ALWAYS_INLINE CodePtr<tag> getCodePtrImpl(const JSC::Opcode opcode, const void* opcodeAddress)
{
    void* opcodeValue = reinterpret_cast<void*>(opcode);
    void* untaggedOpcode = untagAddressDiversifiedCodePtr<BytecodePtrTag>(opcodeValue, opcodeAddress);
    void* retaggedOpcode = tagCodePtr<tag>(untaggedOpcode);
    return CodePtr<tag>::fromTaggedPtr(retaggedOpcode);
}

#if ENABLE(ARM64E) && !ENABLE(COMPUTED_GOTO_OPCODES)
#error ENABLE(ARM64E) requires ENABLE(COMPUTED_GOTO_OPCODES) for getCodePtr (and its variants).
#endif

template<PtrTag tag>
ALWAYS_INLINE CodePtr<tag> getCodePtr(OpcodeID opcodeID)
{
#if ENABLE(COMPUTED_GOTO_OPCODES)
    const JSC::Opcode* opcode = getOpcodeAddress(opcodeID);
    return getCodePtrImpl<tag>(*opcode, opcode);
#else
    return getCodePtrImpl<tag>(getOpcode(opcodeID), nullptr);
#endif
}

template<PtrTag tag>
ALWAYS_INLINE CodePtr<tag> getWide16CodePtr(OpcodeID opcodeID)
{
#if ENABLE(COMPUTED_GOTO_OPCODES)
    const JSC::Opcode* opcode = getOpcodeWide16Address(opcodeID);
    return getCodePtrImpl<tag>(*opcode, opcode);
#else
    return getCodePtrImpl<tag>(getOpcodeWide16(opcodeID), nullptr);
#endif
}

template<PtrTag tag>
ALWAYS_INLINE CodePtr<tag> getWide32CodePtr(OpcodeID opcodeID)
{
#if ENABLE(COMPUTED_GOTO_OPCODES)
    const JSC::Opcode* opcode = getOpcodeWide32Address(opcodeID);
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

template<PtrTag tag>
ALWAYS_INLINE LLIntCode getCodeFunctionPtr(OpcodeID opcodeID)
{
    return reinterpret_cast<LLIntCode>(getCodePtr<tag>(opcodeID).template taggedPtr<>());
}

#if ENABLE(JIT)

template<PtrTag tag>
ALWAYS_INLINE LLIntCode getWide16CodeFunctionPtr(OpcodeID opcodeID)
{
    return reinterpret_cast<LLIntCode>(getWide16CodePtr<tag>(opcodeID).template taggedPtr<>());
}

template<PtrTag tag>
ALWAYS_INLINE LLIntCode getWide32CodeFunctionPtr(OpcodeID opcodeID)
{
    return reinterpret_cast<LLIntCode>(getWide32CodePtr<tag>(opcodeID).template taggedPtr<>());
}
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

inline JSC::Opcode getOpcode(WasmOpcodeID id)
{
#if ENABLE(COMPUTED_GOTO_OPCODES)
    return g_opcodeMap[numOpcodeIDs + id];
#else
    return static_cast<Opcode>(id);
#endif
}

inline JSC::Opcode getOpcodeWide16(WasmOpcodeID id)
{
#if ENABLE(COMPUTED_GOTO_OPCODES)
    return g_opcodeMapWide16[numOpcodeIDs + id];
#else
    UNUSED_PARAM(id);
    RELEASE_ASSERT_NOT_REACHED();
#endif
}

inline JSC::Opcode getOpcodeWide32(WasmOpcodeID id)
{
#if ENABLE(COMPUTED_GOTO_OPCODES)
    return g_opcodeMapWide32[numOpcodeIDs + id];
#else
    UNUSED_PARAM(id);
    RELEASE_ASSERT_NOT_REACHED();
#endif
}

#if ENABLE(COMPUTED_GOTO_OPCODES)
inline const JSC::Opcode* getOpcodeAddress(WasmOpcodeID id)
{
    return &g_opcodeMap[numOpcodeIDs + id];
}

inline const JSC::Opcode* getOpcodeWide16Address(WasmOpcodeID id)
{
    return &g_opcodeMapWide16[numOpcodeIDs + id];
}

inline const JSC::Opcode* getOpcodeWide32Address(WasmOpcodeID id)
{
    return &g_opcodeMapWide32[numOpcodeIDs + id];
}
#endif

template<PtrTag tag>
ALWAYS_INLINE CodePtr<tag> getCodePtr(WasmOpcodeID opcodeID)
{
#if ENABLE(COMPUTED_GOTO_OPCODES)
    const JSC::Opcode* opcode = getOpcodeAddress(opcodeID);
    return getCodePtrImpl<tag>(*opcode, opcode);
#else
    return getCodePtrImpl<tag>(getOpcode(opcodeID), nullptr);
#endif
}

template<PtrTag tag>
ALWAYS_INLINE CodePtr<tag> getWide16CodePtr(WasmOpcodeID opcodeID)
{
#if ENABLE(COMPUTED_GOTO_OPCODES)
    const JSC::Opcode* opcode = getOpcodeWide16Address(opcodeID);
    return getCodePtrImpl<tag>(*opcode, opcode);
#else
    return getCodePtrImpl<tag>(getOpcodeWide16(opcodeID), nullptr);
#endif
}

template<PtrTag tag>
ALWAYS_INLINE CodePtr<tag> getWide32CodePtr(WasmOpcodeID opcodeID)
{
#if ENABLE(COMPUTED_GOTO_OPCODES)
    const JSC::Opcode* opcode = getOpcodeWide32Address(opcodeID);
    return getCodePtrImpl<tag>(*opcode, opcode);
#else
    return getCodePtrImpl<tag>(getOpcodeWide32(opcodeID), nullptr);
#endif
}

template<PtrTag tag>
ALWAYS_INLINE MacroAssemblerCodeRef<tag> getCodeRef(WasmOpcodeID opcodeID)
{
    return MacroAssemblerCodeRef<tag>::createSelfManagedCodeRef(getCodePtr<tag>(opcodeID));
}

template<PtrTag tag>
ALWAYS_INLINE MacroAssemblerCodeRef<tag> getWide16CodeRef(WasmOpcodeID opcodeID)
{
    return MacroAssemblerCodeRef<tag>::createSelfManagedCodeRef(getWide16CodePtr<tag>(opcodeID));
}

template<PtrTag tag>
ALWAYS_INLINE MacroAssemblerCodeRef<tag> getWide32CodeRef(WasmOpcodeID opcodeID)
{
    return MacroAssemblerCodeRef<tag>::createSelfManagedCodeRef(getWide32CodePtr<tag>(opcodeID));
}

template<PtrTag tag>
ALWAYS_INLINE LLIntCode getCodeFunctionPtr(WasmOpcodeID opcodeID)
{
    return reinterpret_cast<LLIntCode>(getCodePtr<tag>(opcodeID).template taggedPtr<>());
}

#if ENABLE(JIT)

template<PtrTag tag>
ALWAYS_INLINE LLIntCode getWide16CodeFunctionPtr(WasmOpcodeID opcodeID)
{
    return reinterpret_cast<LLIntCode>(getWide16CodePtr<tag>(opcodeID).template taggedPtr<>());
}

template<PtrTag tag>
ALWAYS_INLINE LLIntCode getWide32CodeFunctionPtr(WasmOpcodeID opcodeID)
{
    return reinterpret_cast<LLIntCode>(getWide32CodePtr<tag>(opcodeID).template taggedPtr<>());
}
#else // not ENABLE(JIT)
ALWAYS_INLINE void* getCodePtr(WasmOpcodeID id)
{
    return reinterpret_cast<void*>(getOpcode(id));
}

ALWAYS_INLINE void* getWide16CodePtr(WasmOpcodeID id)
{
    return reinterpret_cast<void*>(getOpcodeWide16(id));
}

ALWAYS_INLINE void* getWide32CodePtr(WasmOpcodeID id)
{
    return reinterpret_cast<void*>(getOpcodeWide32(id));
}
#endif // ENABLE(JIT)

#if ENABLE(JIT)
struct Registers {
    static constexpr GPRReg pcGPR = GPRInfo::regT4;
    static constexpr GPRReg pbGPR = GPRInfo::jitDataRegister;
    static constexpr GPRReg metadataTableGPR = GPRInfo::metadataTableRegister;
};
#endif

} } // namespace JSC::LLInt

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
