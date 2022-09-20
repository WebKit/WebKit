/*
 * Copyright (C) 2016 Yusuke Suzuki <utatane.tea@gmail.com>
 * Copyright (C) 2016-2022 Apple Inc. All rights reserved.
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

#include "CallFrameClosure.h"
#include "Exception.h"
#include "FunctionCodeBlock.h"
#include "FunctionExecutable.h"
#include "Instruction.h"
#include "Interpreter.h"
#include "JSCPtrTag.h"
#include "LLIntData.h"
#include "ProtoCallFrameInlines.h"
#include "StackAlignment.h"
#include "UnlinkedCodeBlock.h"
#include "VMTrapsInlines.h"
#include <wtf/UnalignedAccess.h>

namespace JSC {

inline CallFrame* calleeFrameForVarargs(CallFrame* callFrame, unsigned numUsedStackSlots, unsigned argumentCountIncludingThis)
{
    // We want the new frame to be allocated on a stack aligned offset with a stack
    // aligned size. Align the size here.
    argumentCountIncludingThis = WTF::roundUpToMultipleOf(
        stackAlignmentRegisters(),
        argumentCountIncludingThis + CallFrame::headerSizeInRegisters) - CallFrame::headerSizeInRegisters;

    // Align the frame offset here.
    unsigned paddedCalleeFrameOffset = WTF::roundUpToMultipleOf(
        stackAlignmentRegisters(),
        numUsedStackSlots + argumentCountIncludingThis + CallFrame::headerSizeInRegisters);
    return CallFrame::create(callFrame->registers() - paddedCalleeFrameOffset);
}

inline Opcode Interpreter::getOpcode(OpcodeID id)
{
    return LLInt::getOpcode(id);
}

// This function is only available as a debugging tool for development work.
// It is not currently used except in a RELEASE_ASSERT to ensure that it is
// working properly.
inline OpcodeID Interpreter::getOpcodeID(Opcode opcode)
{
#if ENABLE(COMPUTED_GOTO_OPCODES)
    ASSERT(isOpcode(opcode));
#if ENABLE(LLINT_EMBEDDED_OPCODE_ID)
    // The OpcodeID is embedded in the int32_t word preceding the location of
    // the LLInt code for the opcode (see the EMBED_OPCODE_ID_IF_NEEDED macro
    // in LowLevelInterpreter.cpp).
    const void* opcodeAddress = removeCodePtrTag(bitwise_cast<const void*>(opcode));
    const int32_t* opcodeIDAddress = bitwise_cast<int32_t*>(opcodeAddress) - 1;
    OpcodeID opcodeID = static_cast<OpcodeID>(WTF::unalignedLoad<int32_t>(opcodeIDAddress));
    ASSERT(opcodeID < NUMBER_OF_BYTECODE_IDS);
    return opcodeID;
#else
    return opcodeIDTable().get(opcode);
#endif // ENABLE(LLINT_EMBEDDED_OPCODE_ID)

#else // not ENABLE(COMPUTED_GOTO_OPCODES)
    return opcode;
#endif
}

ALWAYS_INLINE JSValue Interpreter::execute(CallFrameClosure& closure)
{
    VM& vm = *closure.vm;
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    ASSERT(!vm.isCollectorBusyOnCurrentThread());
    ASSERT(vm.currentThreadIsHoldingAPILock());

    StackStats::CheckPoint stackCheckPoint;

    if (UNLIKELY(vm.traps().needHandling(VMTraps::NonDebuggerAsyncEvents))) {
        if (vm.hasExceptionsAfterHandlingTraps())
            return throwScope.exception();
    }

    {
        DeferTraps deferTraps(vm); // We can't jettison this code if we're about to run it.

        // Reload CodeBlock since GC can replace CodeBlock owned by Executable.
        CodeBlock* codeBlock;
        closure.functionExecutable->prepareForExecution<FunctionExecutable>(vm, closure.function, closure.scope, CodeForCall, codeBlock);
        RETURN_IF_EXCEPTION(throwScope, checkedReturn(throwScope.exception()));

        ASSERT(codeBlock);
        codeBlock->m_shouldAlwaysBeInlined = false;
        {
            DisallowGC disallowGC; // Ensure no GC happens. GC can replace CodeBlock in Executable.
            closure.protoCallFrame->setCodeBlock(codeBlock);
        }
    }

    // Execute the code:
    throwScope.release();
    JSValue result = closure.functionExecutable->generatedJITCodeForCall()->execute(&vm, closure.protoCallFrame);
    return checkedReturn(result);
}

} // namespace JSC
