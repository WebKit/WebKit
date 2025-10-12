/*
 * Copyright (C) 2011-2025 Apple Inc. All rights reserved.
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

constexpr size_t OpcodeConfigAlignment = CeilingOnPageSize;
constexpr size_t OpcodeConfigSizeToProtect = std::max(CeilingOnPageSize, 16 * KB);

struct OpcodeConfig {
    JSC::Opcode opcodeMap[numOpcodeIDs];
    JSC::Opcode opcodeMapWide16[numOpcodeIDs];
    JSC::Opcode opcodeMapWide32[numOpcodeIDs];

    void* ipint_dispatch_base;
    void* ipint_gc_dispatch_base;
    void* ipint_conversion_dispatch_base;
    void* ipint_simd_dispatch_base;
    void* ipint_atomic_dispatch_base;
};

extern "C" WTF_EXPORT_PRIVATE JSC::Opcode g_opcodeConfigStorage[];

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

inline OpcodeConfig* addressOfOpcodeConfig() { return std::bit_cast<OpcodeConfig*>(&g_opcodeConfigStorage); }

#define g_opcodeConfig (*JSC::LLInt::addressOfOpcodeConfig())

#define g_opcodeMap g_opcodeConfig.opcodeMap
#define g_opcodeMapWide16 g_opcodeConfig.opcodeMapWide16
#define g_opcodeMapWide32 g_opcodeConfig.opcodeMapWide32

class Data {
    friend void initialize();

    friend JSInstruction* exceptionInstructions();
    friend JSC::Opcode* opcodeMap();
    friend JSC::Opcode* opcodeMapWide16();
    friend JSC::Opcode* opcodeMapWide32();
    friend JSC::Opcode getOpcode(OpcodeID);
    friend JSC::Opcode getOpcodeWide16(OpcodeID);
    friend JSC::Opcode getOpcodeWide32(OpcodeID);
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

template<PtrTag tag>
ALWAYS_INLINE CodePtr<tag> getCodePtrImpl(const JSC::Opcode opcode)
{
    void* taggedOpcode = tagCodePtr<tag>(reinterpret_cast<void*>(opcode));
    return CodePtr<tag>::fromTaggedPtr(taggedOpcode);
}

#if ENABLE(ARM64E) && !ENABLE(COMPUTED_GOTO_OPCODES)
#error ENABLE(ARM64E) requires ENABLE(COMPUTED_GOTO_OPCODES) for getCodePtr (and its variants).
#endif

template<PtrTag tag>
ALWAYS_INLINE CodePtr<tag> getCodePtr(OpcodeID opcodeID)
{
    return getCodePtrImpl<tag>(getOpcode(opcodeID));
}

template<PtrTag tag>
ALWAYS_INLINE CodePtr<tag> getWide16CodePtr(OpcodeID opcodeID)
{
    return getCodePtrImpl<tag>(getOpcodeWide16(opcodeID));
}

template<PtrTag tag>
ALWAYS_INLINE CodePtr<tag> getWide32CodePtr(OpcodeID opcodeID)
{
    return getCodePtrImpl<tag>(getOpcodeWide32(opcodeID));
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

#if ENABLE(JIT)
struct Registers {
    static constexpr GPRReg pcGPR = GPRInfo::regT4;
    static constexpr GPRReg pbGPR = GPRInfo::jitDataRegister;
    static constexpr GPRReg metadataTableGPR = GPRInfo::metadataTableRegister;
};
#endif

} } // namespace JSC::LLInt

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
