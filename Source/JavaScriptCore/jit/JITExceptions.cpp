/*
 * Copyright (C) 2012-2018 Apple Inc. All rights reserved.
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
#include "JITExceptions.h"

#include "CallFrame.h"
#include "CatchScope.h"
#include "CodeBlock.h"
#include "Disassembler.h"
#include "EntryFrame.h"
#include "Interpreter.h"
#include "JSCInlines.h"
#include "JSCJSValue.h"
#include "LLIntData.h"
#include "LLIntOpcode.h"
#include "LLIntThunks.h"
#include "Opcode.h"
#include "ShadowChicken.h"
#include "VMInlines.h"

namespace JSC {

void genericUnwind(VM* vm, ExecState* callFrame)
{
    auto scope = DECLARE_CATCH_SCOPE(*vm);
    CallFrame* topJSCallFrame = vm->topJSCallFrame();
    if (Options::breakOnThrow()) {
        CodeBlock* codeBlock = topJSCallFrame->codeBlock();
        dataLog("In call frame ", RawPointer(topJSCallFrame), " for code block ", codeBlock, "\n");
        CRASH();
    }
    
    vm->shadowChicken().log(*vm, topJSCallFrame, ShadowChicken::Packet::throwPacket());

    Exception* exception = scope.exception();
    RELEASE_ASSERT(exception);
    HandlerInfo* handler = vm->interpreter->unwind(*vm, callFrame, exception); // This may update callFrame.

    void* catchRoutine;
    const Instruction* catchPCForInterpreter = nullptr;
    if (handler) {
        // handler->target is meaningless for getting a code offset when catching
        // the exception in a DFG/FTL frame. This bytecode target offset could be
        // something that's in an inlined frame, which means an array access
        // with this bytecode offset in the machine frame is utterly meaningless
        // and can cause an overflow. OSR exit properly exits to handler->target
        // in the proper frame.
        if (!JITCode::isOptimizingJIT(callFrame->codeBlock()->jitType()))
            catchPCForInterpreter = callFrame->codeBlock()->instructions().at(handler->target).ptr();
#if ENABLE(JIT)
        catchRoutine = handler->nativeCode.executableAddress();
#else
        catchRoutine = catchPCForInterpreter->isWide()
            ? LLInt::getWideCodePtr(catchPCForInterpreter->opcodeID())
            : LLInt::getCodePtr(catchPCForInterpreter->opcodeID());
#endif
    } else
        catchRoutine = LLInt::getCodePtr<ExceptionHandlerPtrTag>(handleUncaughtException).executableAddress();

    ASSERT(bitwise_cast<uintptr_t>(callFrame) < bitwise_cast<uintptr_t>(vm->topEntryFrame));

    assertIsTaggedWith(catchRoutine, ExceptionHandlerPtrTag);
    vm->callFrameForCatch = callFrame;
    vm->targetMachinePCForThrow = catchRoutine;
    vm->targetInterpreterPCForThrow = catchPCForInterpreter;
    
    RELEASE_ASSERT(catchRoutine);
}

} // namespace JSC
