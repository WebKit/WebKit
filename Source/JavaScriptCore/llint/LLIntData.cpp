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
Opcode g_opcodeMap[numOpcodeIDs] = { };
Opcode g_opcodeMapWide[numOpcodeIDs] = { };

#if !ENABLE(C_LOOP)
extern "C" void llint_entry(void*, void*);
#endif

void initialize()
{
#if ENABLE(C_LOOP)
    CLoop::initialize();

#else // !ENABLE(C_LOOP)
    llint_entry(&g_opcodeMap, &g_opcodeMapWide);

    for (int i = 0; i < numOpcodeIDs; ++i) {
        g_opcodeMap[i] = tagCodePtr(g_opcodeMap[i], BytecodePtrTag);
        g_opcodeMapWide[i] = tagCodePtr(g_opcodeMapWide[i], BytecodePtrTag);
    }

    ASSERT(llint_throw_from_slow_path_trampoline < UINT8_MAX);
    for (int i = 0; i < maxOpcodeLength + 1; ++i)
        Data::s_exceptionInstructions[i] = llint_throw_from_slow_path_trampoline;
#endif // ENABLE(C_LOOP)
}

IGNORE_CLANG_WARNINGS_BEGIN("missing-noreturn")
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
    STATIC_ASSERT(CallFrameSlot::argumentCount * sizeof(Register) == CallFrameSlot::callee * sizeof(Register) + SlotSize);
    STATIC_ASSERT(CallFrameSlot::thisArgument * sizeof(Register) == CallFrameSlot::argumentCount * sizeof(Register) + SlotSize);
    STATIC_ASSERT(CallFrame::headerSizeInRegisters == CallFrameSlot::thisArgument);

    ASSERT(CallFrame::argumentOffsetIncludingThis(0) == CallFrameSlot::thisArgument);

#if CPU(BIG_ENDIAN)
    STATIC_ASSERT(TagOffset == 0);
    STATIC_ASSERT(PayloadOffset == 4);
#else
    STATIC_ASSERT(TagOffset == 4);
    STATIC_ASSERT(PayloadOffset == 0);
#endif
#if USE(JSVALUE32_64)
    STATIC_ASSERT(JSValue::Int32Tag == static_cast<unsigned>(-1));
    STATIC_ASSERT(JSValue::BooleanTag == static_cast<unsigned>(-2));
    STATIC_ASSERT(JSValue::NullTag == static_cast<unsigned>(-3));
    STATIC_ASSERT(JSValue::UndefinedTag == static_cast<unsigned>(-4));
    STATIC_ASSERT(JSValue::CellTag == static_cast<unsigned>(-5));
    STATIC_ASSERT(JSValue::EmptyValueTag == static_cast<unsigned>(-6));
    STATIC_ASSERT(JSValue::DeletedValueTag == static_cast<unsigned>(-7));
    STATIC_ASSERT(JSValue::LowestTag == static_cast<unsigned>(-7));
#else
    STATIC_ASSERT(TagBitTypeOther == 0x2);
    STATIC_ASSERT(TagBitBool == 0x4);
    STATIC_ASSERT(TagBitUndefined == 0x8);
    STATIC_ASSERT(ValueEmpty == 0x0);
    STATIC_ASSERT(ValueFalse == (TagBitTypeOther | TagBitBool));
    STATIC_ASSERT(ValueTrue == (TagBitTypeOther | TagBitBool | 1));
    STATIC_ASSERT(ValueUndefined == (TagBitTypeOther | TagBitUndefined));
    STATIC_ASSERT(ValueNull == TagBitTypeOther);
#endif

#if ENABLE(C_LOOP)
    ASSERT(CodeBlock::llintBaselineCalleeSaveSpaceAsVirtualRegisters() == 1);
#elif USE(JSVALUE32_64)
    ASSERT(!CodeBlock::llintBaselineCalleeSaveSpaceAsVirtualRegisters());
#elif (CPU(X86_64) && !OS(WINDOWS))  || CPU(ARM64)
    ASSERT(CodeBlock::llintBaselineCalleeSaveSpaceAsVirtualRegisters() == 4);
#elif (CPU(X86_64) && OS(WINDOWS))
    ASSERT(CodeBlock::llintBaselineCalleeSaveSpaceAsVirtualRegisters() == 4);
#endif

    ASSERT(!(reinterpret_cast<ptrdiff_t>((reinterpret_cast<WriteBarrier<JSCell>*>(0x4000)->slot())) - 0x4000));

    // FIXME: make these assertions less horrible.
#if !ASSERT_DISABLED
    Vector<int> testVector;
    testVector.resize(42);
    ASSERT(bitwise_cast<uint32_t*>(&testVector)[sizeof(void*)/sizeof(uint32_t) + 1] == 42);
    ASSERT(bitwise_cast<int**>(&testVector)[0] == testVector.begin());
#endif

    {
        ArithProfile arithProfile;
        arithProfile.lhsSawInt32();
        arithProfile.rhsSawInt32();
        ASSERT(arithProfile.bits() == ArithProfile::observedBinaryIntInt().bits());
        STATIC_ASSERT(ArithProfile::observedBinaryIntInt().lhsObservedType().isOnlyInt32());
        STATIC_ASSERT(ArithProfile::observedBinaryIntInt().rhsObservedType().isOnlyInt32());
    }
    {
        ArithProfile arithProfile;
        arithProfile.lhsSawNumber();
        arithProfile.rhsSawInt32();
        ASSERT(arithProfile.bits() == ArithProfile::observedBinaryNumberInt().bits());
        STATIC_ASSERT(ArithProfile::observedBinaryNumberInt().lhsObservedType().isOnlyNumber());
        STATIC_ASSERT(ArithProfile::observedBinaryNumberInt().rhsObservedType().isOnlyInt32());
    }
    {
        ArithProfile arithProfile;
        arithProfile.lhsSawNumber();
        arithProfile.rhsSawNumber();
        ASSERT(arithProfile.bits() == ArithProfile::observedBinaryNumberNumber().bits());
        STATIC_ASSERT(ArithProfile::observedBinaryNumberNumber().lhsObservedType().isOnlyNumber());
        STATIC_ASSERT(ArithProfile::observedBinaryNumberNumber().rhsObservedType().isOnlyNumber());
    }
    {
        ArithProfile arithProfile;
        arithProfile.lhsSawInt32();
        arithProfile.rhsSawNumber();
        ASSERT(arithProfile.bits() == ArithProfile::observedBinaryIntNumber().bits());
        STATIC_ASSERT(ArithProfile::observedBinaryIntNumber().lhsObservedType().isOnlyInt32());
        STATIC_ASSERT(ArithProfile::observedBinaryIntNumber().rhsObservedType().isOnlyNumber());
    }
}
IGNORE_CLANG_WARNINGS_END

} } // namespace JSC::LLInt
