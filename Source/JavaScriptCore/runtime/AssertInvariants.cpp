/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include "AssertInvariants.h"

#include "ArithProfile.h"
#include "BaselineJITCode.h"
#include "CodeBlock.h"
#include "DFGJITCode.h"

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

void assertInvariants()
{
    // Assertions to match LowLevelInterpreter.asm. If you change any of this code, be
    // prepared to change LowLevelInterpreter.asm as well!!
    {
#if USE(JSVALUE64)
        const ptrdiff_t CallFrameHeaderSlots = 5;
#else // USE(JSVALUE64) // i.e. 32-bit version
        const ptrdiff_t CallFrameHeaderSlots = 4;
#endif
        const ptrdiff_t MachineRegisterSize = sizeof(CPURegister);
        const ptrdiff_t SlotSize = 8;

        static_assert(sizeof(Register) == SlotSize);
        static_assert(CallFrame::headerSizeInRegisters == CallFrameHeaderSlots);

        static_assert(!CallFrame::callerFrameOffset());
        static_assert(CallerFrameAndPC::sizeInRegisters == (MachineRegisterSize * 2) / SlotSize);
        static_assert(CallFrame::returnPCOffset() == CallFrame::callerFrameOffset() + MachineRegisterSize);
        static_assert(static_cast<std::underlying_type_t<CallFrameSlot>>(CallFrameSlot::codeBlock) * sizeof(Register) == CallFrame::returnPCOffset() + MachineRegisterSize);
        static_assert(CallFrameSlot::callee * sizeof(Register) == CallFrameSlot::codeBlock * sizeof(Register) + SlotSize);
        static_assert(CallFrameSlot::argumentCountIncludingThis * sizeof(Register) == CallFrameSlot::callee * sizeof(Register) + SlotSize);
        static_assert(CallFrameSlot::thisArgument * sizeof(Register) == CallFrameSlot::argumentCountIncludingThis * sizeof(Register) + SlotSize);
        static_assert(CallFrame::headerSizeInRegisters == CallFrameSlot::thisArgument);

        static_assert(CallFrame::argumentOffsetIncludingThis(0) == CallFrameSlot::thisArgument);

#if CPU(BIG_ENDIAN)
        static_assert(TagOffset == 0);
        static_assert(PayloadOffset == 4);
#else
        static_assert(TagOffset == 4);
        static_assert(PayloadOffset == 0);
#endif

#if ENABLE(C_LOOP)
        ASSERT(CodeBlock::llintBaselineCalleeSaveSpaceAsVirtualRegisters() == 1);
#elif USE(JSVALUE32_64)
        ASSERT(CodeBlock::llintBaselineCalleeSaveSpaceAsVirtualRegisters() == 1);
#elif CPU(X86_64) || CPU(ARM64)
        ASSERT(CodeBlock::llintBaselineCalleeSaveSpaceAsVirtualRegisters() == 4);
#endif

        ASSERT(!(reinterpret_cast<ptrdiff_t>((reinterpret_cast<WriteBarrier<JSCell>*>(0x4000)->slot())) - 0x4000));
    }

    // FIXME: make these assertions less horrible.
#if ASSERT_ENABLED
    Vector<int> testVector;
    testVector.resize(42);
    ASSERT(std::bit_cast<uint32_t*>(&testVector)[sizeof(void*) / sizeof(uint32_t) + 1] == 42);
    ASSERT(std::bit_cast<int**>(&testVector)[0] == testVector.begin());
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

#if ENABLE(DFG_JIT)
    // We share the same layout for particular fields in all JITData to make our data IC assume this.
    static_assert(BaselineJITData::offsetOfGlobalObject() == DFG::JITData::offsetOfGlobalObject());
    static_assert(BaselineJITData::offsetOfStackOffset() == DFG::JITData::offsetOfStackOffset());
#endif
}

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
