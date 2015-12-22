/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#include "BytecodeConventions.h"
#include "CodeBlock.h"
#include "CodeType.h"
#include "Instruction.h"
#include "JSScope.h"
#include "LLIntCLoop.h"
#include "MaxFrameExtentForSlowPathCall.h"
#include "Opcode.h"
#include "PropertyOffset.h"
#include "WriteBarrier.h"

#define STATIC_ASSERT(cond) static_assert(cond, "LLInt assumes " #cond)

namespace JSC { namespace LLInt {

Instruction* Data::s_exceptionInstructions = 0;
Opcode Data::s_opcodeMap[numOpcodeIDs] = { };

#if ENABLE(JIT)
extern "C" void llint_entry(void*);
#endif

void initialize()
{
    Data::s_exceptionInstructions = new Instruction[maxOpcodeLength + 1];

#if !ENABLE(JIT)
    CLoop::initialize();

#else // ENABLE(JIT)
    llint_entry(&Data::s_opcodeMap);

    for (int i = 0; i < maxOpcodeLength + 1; ++i)
        Data::s_exceptionInstructions[i].u.pointer =
            LLInt::getCodePtr(llint_throw_from_slow_path_trampoline);
#endif // ENABLE(JIT)
}

#if COMPILER(CLANG)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
#endif
void Data::performAssertions(VM& vm)
{
    UNUSED_PARAM(vm);
    
    // Assertions to match LowLevelInterpreter.asm.  If you change any of this code, be
    // prepared to change LowLevelInterpreter.asm as well!!

#if USE(JSVALUE64)
    const ptrdiff_t PtrSize = 8;
    const ptrdiff_t CallFrameHeaderSlots = 5;
#else // USE(JSVALUE64) // i.e. 32-bit version
    const ptrdiff_t PtrSize = 4;
    const ptrdiff_t CallFrameHeaderSlots = 4;
#endif
    const ptrdiff_t SlotSize = 8;

    STATIC_ASSERT(sizeof(void*) == PtrSize);
    STATIC_ASSERT(sizeof(Register) == SlotSize);
    STATIC_ASSERT(JSStack::CallFrameHeaderSize == CallFrameHeaderSlots);

    ASSERT(!CallFrame::callerFrameOffset());
    STATIC_ASSERT(JSStack::CallerFrameAndPCSize == (PtrSize * 2) / SlotSize);
    ASSERT(CallFrame::returnPCOffset() == CallFrame::callerFrameOffset() + PtrSize);
    ASSERT(JSStack::CodeBlock * sizeof(Register) == CallFrame::returnPCOffset() + PtrSize);
    STATIC_ASSERT(JSStack::Callee * sizeof(Register) == JSStack::CodeBlock * sizeof(Register) + SlotSize);
    STATIC_ASSERT(JSStack::ArgumentCount * sizeof(Register) == JSStack::Callee * sizeof(Register) + SlotSize);
    STATIC_ASSERT(JSStack::ThisArgument * sizeof(Register) == JSStack::ArgumentCount * sizeof(Register) + SlotSize);
    STATIC_ASSERT(JSStack::CallFrameHeaderSize == JSStack::ThisArgument);

    ASSERT(CallFrame::argumentOffsetIncludingThis(0) == JSStack::ThisArgument);

#if CPU(BIG_ENDIAN)
    ASSERT(OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.tag) == 0);
    ASSERT(OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.payload) == 4);
#else
    ASSERT(OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.tag) == 4);
    ASSERT(OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.payload) == 0);
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
#if (CPU(X86_64) && !OS(WINDOWS)) || CPU(ARM64) || !ENABLE(JIT)
    STATIC_ASSERT(!maxFrameExtentForSlowPathCall);
#elif CPU(ARM) || CPU(SH4)
    STATIC_ASSERT(maxFrameExtentForSlowPathCall == 24);
#elif CPU(X86) || CPU(MIPS)
    STATIC_ASSERT(maxFrameExtentForSlowPathCall == 40);
#elif CPU(X86_64) && OS(WINDOWS)
    STATIC_ASSERT(maxFrameExtentForSlowPathCall == 64);
#endif

#if !ENABLE(JIT) || USE(JSVALUE32_64)
    ASSERT(!CodeBlock::llintBaselineCalleeSaveSpaceAsVirtualRegisters());
#elif (CPU(X86_64) && !OS(WINDOWS))  || CPU(ARM64)
    ASSERT(CodeBlock::llintBaselineCalleeSaveSpaceAsVirtualRegisters() == 3);
#elif (CPU(X86_64) && OS(WINDOWS))
    ASSERT(CodeBlock::llintBaselineCalleeSaveSpaceAsVirtualRegisters() == 3);
#endif
    
    STATIC_ASSERT(StringType == 6);
    STATIC_ASSERT(SymbolType == 7);
    STATIC_ASSERT(ObjectType == 21);
    STATIC_ASSERT(FinalObjectType == 22);
    STATIC_ASSERT(MasqueradesAsUndefined == 1);
    STATIC_ASSERT(ImplementsDefaultHasInstance == 2);
    STATIC_ASSERT(FirstConstantRegisterIndex == 0x40000000);
    STATIC_ASSERT(GlobalCode == 0);
    STATIC_ASSERT(EvalCode == 1);
    STATIC_ASSERT(FunctionCode == 2);
    STATIC_ASSERT(ModuleCode == 3);

    ASSERT(!(reinterpret_cast<ptrdiff_t>((reinterpret_cast<WriteBarrier<JSCell>*>(0x4000)->slot())) - 0x4000));
    static_assert(PutByIdPrimaryTypeMask == 0x6, "LLInt assumes PutByIdPrimaryTypeMask is == 0x6");
    static_assert(PutByIdPrimaryTypeSecondary == 0x0, "LLInt assumes PutByIdPrimaryTypeSecondary is == 0x0");
    static_assert(PutByIdPrimaryTypeObjectWithStructure == 0x2, "LLInt assumes PutByIdPrimaryTypeObjectWithStructure is == 0x2");
    static_assert(PutByIdPrimaryTypeObjectWithStructureOrOther == 0x4, "LLInt assumes PutByIdPrimaryTypeObjectWithStructureOrOther is == 0x4");
    static_assert(PutByIdSecondaryTypeMask == -0x8, "LLInt assumes PutByIdSecondaryTypeMask is == -0x8");
    static_assert(PutByIdSecondaryTypeBottom == 0x0, "LLInt assumes PutByIdSecondaryTypeBottom is == 0x0");
    static_assert(PutByIdSecondaryTypeBoolean == 0x8, "LLInt assumes PutByIdSecondaryTypeBoolean is == 0x8");
    static_assert(PutByIdSecondaryTypeOther == 0x10, "LLInt assumes PutByIdSecondaryTypeOther is == 0x10");
    static_assert(PutByIdSecondaryTypeInt32 == 0x18, "LLInt assumes PutByIdSecondaryTypeInt32 is == 0x18");
    static_assert(PutByIdSecondaryTypeNumber == 0x20, "LLInt assumes PutByIdSecondaryTypeNumber is == 0x20");
    static_assert(PutByIdSecondaryTypeString == 0x28, "LLInt assumes PutByIdSecondaryTypeString is == 0x28");
    static_assert(PutByIdSecondaryTypeSymbol == 0x30, "LLInt assumes PutByIdSecondaryTypeSymbol is == 0x30");
    static_assert(PutByIdSecondaryTypeObject == 0x38, "LLInt assumes PutByIdSecondaryTypeObject is == 0x38");
    static_assert(PutByIdSecondaryTypeObjectOrOther == 0x40, "LLInt assumes PutByIdSecondaryTypeObjectOrOther is == 0x40");
    static_assert(PutByIdSecondaryTypeTop == 0x48, "LLInt assumes PutByIdSecondaryTypeTop is == 0x48");

    static_assert(GlobalProperty == 0, "LLInt assumes GlobalProperty ResultType is == 0");
    static_assert(GlobalVar == 1, "LLInt assumes GlobalVar ResultType is == 1");
    static_assert(GlobalLexicalVar == 2, "LLInt assumes GlobalLexicalVar ResultType is == 2");
    static_assert(ClosureVar == 3, "LLInt assumes ClosureVar ResultType is == 3");
    static_assert(LocalClosureVar == 4, "LLInt assumes LocalClosureVar ResultType is == 4");
    static_assert(ModuleVar == 5, "LLInt assumes ModuleVar ResultType is == 5");
    static_assert(GlobalPropertyWithVarInjectionChecks == 6, "LLInt assumes GlobalPropertyWithVarInjectionChecks ResultType is == 6");
    static_assert(GlobalVarWithVarInjectionChecks == 7, "LLInt assumes GlobalVarWithVarInjectionChecks ResultType is == 7");
    static_assert(GlobalLexicalVarWithVarInjectionChecks == 8, "LLInt assumes GlobalLexicalVarWithVarInjectionChecks ResultType is == 8");
    static_assert(ClosureVarWithVarInjectionChecks == 9, "LLInt assumes ClosureVarWithVarInjectionChecks ResultType is == 9");

    static_assert(InitializationMode::Initialization == 0, "LLInt assumes that InitializationMode::Initialization is 0");
    
    STATIC_ASSERT(GetPutInfo::typeBits == 0x3ff);
    STATIC_ASSERT(GetPutInfo::initializationShift == 10);
    STATIC_ASSERT(GetPutInfo::initializationBits == 0xffc00);

    STATIC_ASSERT(MarkedBlock::blockMask == ~static_cast<decltype(MarkedBlock::blockMask)>(0x3fff));

    // FIXME: make these assertions less horrible.
#if !ASSERT_DISABLED
    Vector<int> testVector;
    testVector.resize(42);
    ASSERT(bitwise_cast<uint32_t*>(&testVector)[sizeof(void*)/sizeof(uint32_t) + 1] == 42);
    ASSERT(bitwise_cast<int**>(&testVector)[0] == testVector.begin());
#endif

    ASSERT(StringImpl::s_hashFlag8BitBuffer == 8);
}
#if COMPILER(CLANG)
#pragma clang diagnostic pop
#endif

} } // namespace JSC::LLInt
