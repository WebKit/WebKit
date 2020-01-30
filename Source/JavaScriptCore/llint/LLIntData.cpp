/*
 * Copyright (C) 2011-2018 Apple Inc. All rights reserved.
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

#include "config.h"
#include "LLIntData.h"

#include "ArithProfile.h"
#include "BytecodeConventions.h"
#include "CodeBlock.h"
#include "CodeType.h"
#include "Instruction.h"
#include "JSScope.h"
#include "LLIntCLoop.h"
#include "MaxFrameExtentForSlowPathCall.h"
#include "Opcode.h"
#include "PropertyOffset.h"
#include "ShadowChicken.h"
#include "WriteBarrier.h"

#define STATIC_ASSERT(cond) static_assert(cond, "LLInt assumes " #cond)


namespace JSC {

namespace LLInt {


uint8_t Data::s_exceptionInstructions[maxOpcodeLength + 1] = { };
uint8_t Data::s_wasmExceptionInstructions[maxOpcodeLength + 1] = { };
Opcode g_opcodeMap[numOpcodeIDs + numWasmOpcodeIDs] = { };
Opcode g_opcodeMapWide16[numOpcodeIDs + numWasmOpcodeIDs] = { };
Opcode g_opcodeMapWide32[numOpcodeIDs + numWasmOpcodeIDs] = { };

#if !ENABLE(C_LOOP)
extern "C" void llint_entry(void*, void*, void*);

#if ENABLE(WEBASSEMBLY)
extern "C" void wasm_entry(void*, void*, void*);
#endif // ENABLE(WEBASSEMBLY)

#endif // !ENABLE(C_LOOP)

void initialize()
{
#if ENABLE(C_LOOP)
    CLoop::initialize();

#else // !ENABLE(C_LOOP)
    llint_entry(&g_opcodeMap, &g_opcodeMapWide16, &g_opcodeMapWide32);

#if ENABLE(WEBASSEMBLY)
    wasm_entry(&g_opcodeMap[numOpcodeIDs], &g_opcodeMapWide16[numOpcodeIDs], &g_opcodeMapWide32[numOpcodeIDs]);
#endif // ENABLE(WEBASSEMBLY)

    for (int i = 0; i < numOpcodeIDs + numWasmOpcodeIDs; ++i) {
        g_opcodeMap[i] = tagCodePtr(g_opcodeMap[i], BytecodePtrTag);
        g_opcodeMapWide16[i] = tagCodePtr(g_opcodeMapWide16[i], BytecodePtrTag);
        g_opcodeMapWide32[i] = tagCodePtr(g_opcodeMapWide32[i], BytecodePtrTag);
    }

    ASSERT(llint_throw_from_slow_path_trampoline < UINT8_MAX);
    for (unsigned i = 0; i < maxOpcodeLength + 1; ++i) {
        Data::s_exceptionInstructions[i] = llint_throw_from_slow_path_trampoline;
        Data::s_wasmExceptionInstructions[i] = wasm_throw_from_slow_path_trampoline;
    }
#endif // ENABLE(C_LOOP)
}

IGNORE_WARNINGS_BEGIN("missing-noreturn")
void Data::performAssertions(VM& vm)
{
    UNUSED_PARAM(vm);
    
    // Assertions to match LowLevelInterpreter.asm.  If you change any of this code, be
    // prepared to change LowLevelInterpreter.asm as well!!

#if USE(JSVALUE64)
    const ptrdiff_t CallFrameHeaderSlots = 5;
#else // USE(JSVALUE64) // i.e. 32-bit version
    const ptrdiff_t CallFrameHeaderSlots = 4;
#endif
    const ptrdiff_t MachineRegisterSize = sizeof(CPURegister);
    const ptrdiff_t SlotSize = 8;

    STATIC_ASSERT(sizeof(Register) == SlotSize);
    STATIC_ASSERT(CallFrame::headerSizeInRegisters == CallFrameHeaderSlots);

    ASSERT(!CallFrame::callerFrameOffset());
    STATIC_ASSERT(CallerFrameAndPC::sizeInRegisters == (MachineRegisterSize * 2) / SlotSize);
    ASSERT(CallFrame::returnPCOffset() == CallFrame::callerFrameOffset() + MachineRegisterSize);
    ASSERT(CallFrameSlot::codeBlock * sizeof(Register) == CallFrame::returnPCOffset() + MachineRegisterSize);
    STATIC_ASSERT(CallFrameSlot::callee * sizeof(Register) == CallFrameSlot::codeBlock * sizeof(Register) + SlotSize);
    STATIC_ASSERT(CallFrameSlot::argumentCountIncludingThis * sizeof(Register) == CallFrameSlot::callee * sizeof(Register) + SlotSize);
    STATIC_ASSERT(CallFrameSlot::thisArgument * sizeof(Register) == CallFrameSlot::argumentCountIncludingThis * sizeof(Register) + SlotSize);
    STATIC_ASSERT(CallFrame::headerSizeInRegisters == CallFrameSlot::thisArgument);

    ASSERT(CallFrame::argumentOffsetIncludingThis(0) == CallFrameSlot::thisArgument);

#if CPU(BIG_ENDIAN)
    STATIC_ASSERT(TagOffset == 0);
    STATIC_ASSERT(PayloadOffset == 4);
#else
    STATIC_ASSERT(TagOffset == 4);
    STATIC_ASSERT(PayloadOffset == 0);
#endif

#if ENABLE(C_LOOP)
    ASSERT(CodeBlock::llintBaselineCalleeSaveSpaceAsVirtualRegisters() == 1);
#elif USE(JSVALUE32_64)
    ASSERT(CodeBlock::llintBaselineCalleeSaveSpaceAsVirtualRegisters() == 1);
#elif (CPU(X86_64) && !OS(WINDOWS))  || CPU(ARM64)
    ASSERT(CodeBlock::llintBaselineCalleeSaveSpaceAsVirtualRegisters() == 4);
#elif (CPU(X86_64) && OS(WINDOWS))
    ASSERT(CodeBlock::llintBaselineCalleeSaveSpaceAsVirtualRegisters() == 4);
#endif

    ASSERT(!(reinterpret_cast<ptrdiff_t>((reinterpret_cast<WriteBarrier<JSCell>*>(0x4000)->slot())) - 0x4000));

    // FIXME: make these assertions less horrible.
#if ASSERT_ENABLED
    Vector<int> testVector;
    testVector.resize(42);
    ASSERT(bitwise_cast<uint32_t*>(&testVector)[sizeof(void*)/sizeof(uint32_t) + 1] == 42);
    ASSERT(bitwise_cast<int**>(&testVector)[0] == testVector.begin());
#endif

    {
        UnaryArithProfile arithProfile;
        arithProfile.argSawInt32();
        ASSERT(arithProfile.bits() == UnaryArithProfile::observedIntBits());
        ASSERT(arithProfile.argObservedType().isOnlyInt32());
    }
    {
        UnaryArithProfile arithProfile;
        arithProfile.argSawNumber();
        ASSERT(arithProfile.bits() == UnaryArithProfile::observedNumberBits());
        ASSERT(arithProfile.argObservedType().isOnlyNumber());
    }

    {
        BinaryArithProfile arithProfile;
        arithProfile.lhsSawInt32();
        arithProfile.rhsSawInt32();
        ASSERT(arithProfile.bits() == BinaryArithProfile::observedIntIntBits());
        ASSERT(arithProfile.lhsObservedType().isOnlyInt32());
        ASSERT(arithProfile.rhsObservedType().isOnlyInt32());
    }
    {
        BinaryArithProfile arithProfile;
        arithProfile.lhsSawNumber();
        arithProfile.rhsSawInt32();
        ASSERT(arithProfile.bits() == BinaryArithProfile::observedNumberIntBits());
        ASSERT(arithProfile.lhsObservedType().isOnlyNumber());
        ASSERT(arithProfile.rhsObservedType().isOnlyInt32());
    }
    {
        BinaryArithProfile arithProfile;
        arithProfile.lhsSawNumber();
        arithProfile.rhsSawNumber();
        ASSERT(arithProfile.bits() == BinaryArithProfile::observedNumberNumberBits());
        ASSERT(arithProfile.lhsObservedType().isOnlyNumber());
        ASSERT(arithProfile.rhsObservedType().isOnlyNumber());
    }
    {
        BinaryArithProfile arithProfile;
        arithProfile.lhsSawInt32();
        arithProfile.rhsSawNumber();
        ASSERT(arithProfile.bits() == BinaryArithProfile::observedIntNumberBits());
        ASSERT(arithProfile.lhsObservedType().isOnlyInt32());
        ASSERT(arithProfile.rhsObservedType().isOnlyNumber());
    }
}
IGNORE_WARNINGS_END

} } // namespace JSC::LLInt
