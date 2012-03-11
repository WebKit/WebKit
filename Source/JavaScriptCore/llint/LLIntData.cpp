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

#if ENABLE(LLINT)

#include "BytecodeConventions.h"
#include "CodeType.h"
#include "Instruction.h"
#include "LowLevelInterpreter.h"
#include "Opcode.h"

namespace JSC { namespace LLInt {

Data::Data()
    : m_exceptionInstructions(new Instruction[maxOpcodeLength + 1])
    , m_opcodeMap(new Opcode[numOpcodeIDs])
{
    for (int i = 0; i < maxOpcodeLength + 1; ++i)
        m_exceptionInstructions[i].u.pointer = bitwise_cast<void*>(&llint_throw_from_slow_path_trampoline);
#define OPCODE_ENTRY(opcode, length) m_opcodeMap[opcode] = bitwise_cast<void*>(&llint_##opcode);
    FOR_EACH_OPCODE_ID(OPCODE_ENTRY);
#undef OPCODE_ENTRY
}

#if COMPILER(CLANG)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
#endif
void Data::performAssertions(JSGlobalData& globalData)
{
    UNUSED_PARAM(globalData);
    
    // Assertions to match LowLevelInterpreter.asm.  If you change any of this code, be
    // prepared to change LowLevelInterpreter.asm as well!!
    ASSERT(RegisterFile::CallFrameHeaderSize * 8 == 48);
    ASSERT(RegisterFile::ArgumentCount * 8 == -48);
    ASSERT(RegisterFile::CallerFrame * 8 == -40);
    ASSERT(RegisterFile::Callee * 8 == -32);
    ASSERT(RegisterFile::ScopeChain * 8 == -24);
    ASSERT(RegisterFile::ReturnPC * 8 == -16);
    ASSERT(RegisterFile::CodeBlock * 8 == -8);
    ASSERT(CallFrame::argumentOffsetIncludingThis(0) == -RegisterFile::CallFrameHeaderSize - 1);
#if CPU(BIG_ENDIAN)
    ASSERT(OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.tag) == 0);
    ASSERT(OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.payload) == 4);
#else
    ASSERT(OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.tag) == 4);
    ASSERT(OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.payload) == 0);
#endif
#if USE(JSVALUE32_64)
    ASSERT(JSValue::Int32Tag == -1);
    ASSERT(JSValue::BooleanTag == -2);
    ASSERT(JSValue::NullTag == -3);
    ASSERT(JSValue::UndefinedTag == -4);
    ASSERT(JSValue::CellTag == -5);
    ASSERT(JSValue::EmptyValueTag == -6);
    ASSERT(JSValue::DeletedValueTag == -7);
    ASSERT(JSValue::LowestTag == -7);
#else
    ASSERT(TagBitTypeOther == 0x2);
    ASSERT(TagBitBool == 0x4);
    ASSERT(TagBitUndefined == 0x8);
    ASSERT(ValueEmpty == 0x0);
    ASSERT(ValueFalse == (TagBitTypeOther | TagBitBool));
    ASSERT(ValueTrue == (TagBitTypeOther | TagBitBool | 1));
    ASSERT(ValueUndefined == (TagBitTypeOther | TagBitUndefined));
    ASSERT(ValueNull == TagBitTypeOther);
#endif
    ASSERT(StringType == 5);
    ASSERT(ObjectType == 13);
    ASSERT(MasqueradesAsUndefined == 1);
    ASSERT(ImplementsHasInstance == 2);
    ASSERT(ImplementsDefaultHasInstance == 8);
#if USE(JSVALUE64)
    ASSERT(&globalData.heap.allocatorForObjectWithoutDestructor(sizeof(JSFinalObject)) - &globalData.heap.firstAllocatorWithoutDestructors() == 1);
#else
    ASSERT(&globalData.heap.allocatorForObjectWithoutDestructor(sizeof(JSFinalObject)) - &globalData.heap.firstAllocatorWithoutDestructors() == 3);
#endif
    ASSERT(FirstConstantRegisterIndex == 0x40000000);
    ASSERT(GlobalCode == 0);
    ASSERT(EvalCode == 1);
    ASSERT(FunctionCode == 2);
    
    // FIXME: make these assertions less horrible.
#if !ASSERT_DISABLED
    Vector<int> testVector;
    testVector.resize(42);
    ASSERT(bitwise_cast<size_t*>(&testVector)[0] == 42);
    ASSERT(bitwise_cast<int**>(&testVector)[1] == testVector.begin());
#endif

    ASSERT(StringImpl::s_hashFlag8BitBuffer == 64);
}
#if COMPILER(CLANG)
#pragma clang diagnostic pop
#endif

Data::~Data()
{
    delete[] m_exceptionInstructions;
    delete[] m_opcodeMap;
}

} } // namespace JSC::LLInt

#endif // ENABLE(LLINT)
