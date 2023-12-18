/*
 * Copyright (C) 2012-2023 Apple Inc. All rights reserved.
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
#include "Interpreter.h"
#include "JSCJSValueInlines.h"
#include "LLIntData.h"
#include "LLIntExceptions.h"
#include "Opcode.h"
#include "ShadowChicken.h"
#include "VMInlines.h"

namespace JSC {

void genericUnwind(VM& vm, CallFrame* callFrame)
{
    auto scope = DECLARE_CATCH_SCOPE(vm);
    CallFrame* topJSCallFrame = vm.topJSCallFrame();
    if (UNLIKELY(Options::breakOnThrow())) {
        CodeBlock* codeBlock = topJSCallFrame->isNativeCalleeFrame() ? nullptr : topJSCallFrame->codeBlock();
        dataLog("In call frame ", RawPointer(topJSCallFrame), " for code block ", codeBlock, "\n");
        WTFBreakpointTrap();
    }
    
    if (auto* shadowChicken = vm.shadowChicken())
        shadowChicken->log(vm, topJSCallFrame, ShadowChicken::Packet::throwPacket());

    Exception* exception = scope.exception();
    RELEASE_ASSERT(exception);
    CatchInfo handler = vm.interpreter.unwind(vm, callFrame, exception); // This may update callFrame.

    void* catchRoutine = nullptr;
    void* dispatchAndCatchRoutine = nullptr;
    JSOrWasmInstruction catchPCForInterpreter = { static_cast<JSInstruction*>(nullptr) };
    uintptr_t catchMetadataPCForInterpreter = 0;
    uint32_t tryDepthForThrow = 0;
    if (handler.m_valid) {
        catchPCForInterpreter = handler.m_catchPCForInterpreter;
        catchMetadataPCForInterpreter = handler.m_catchMetadataPCForInterpreter;
        tryDepthForThrow = handler.m_tryDepthForThrow;
#if ENABLE(JIT)
        catchRoutine = handler.m_nativeCode.taggedPtr();
        if (handler.m_nativeCodeForDispatchAndCatch)
            dispatchAndCatchRoutine = handler.m_nativeCodeForDispatchAndCatch.taggedPtr();
#else
        auto getCatchRoutine = [](const auto* pc) {
            if (pc->isWide32())
                return LLInt::getWide32CodePtr(pc->opcodeID());
            if (pc->isWide16())
                return LLInt::getWide16CodePtr(pc->opcodeID());
            return LLInt::getCodePtr(pc->opcodeID());
        };

        ASSERT_WITH_MESSAGE(!std::holds_alternative<uintptr_t>(catchPCForInterpreter), "IPInt does not support no JIT");
        catchRoutine = std::holds_alternative<const JSInstruction*>(catchPCForInterpreter)
            ? getCatchRoutine(std::get<const JSInstruction*>(catchPCForInterpreter))
            : getCatchRoutine(std::get<const WasmInstruction*>(catchPCForInterpreter));
#endif
    } else
        catchRoutine = LLInt::handleUncaughtException(vm).code().taggedPtr();

    ASSERT(bitwise_cast<uintptr_t>(callFrame) < bitwise_cast<uintptr_t>(vm.topEntryFrame));

    assertIsTaggedWith<ExceptionHandlerPtrTag>(catchRoutine);
    vm.callFrameForCatch = callFrame;
    vm.targetMachinePCForThrow = catchRoutine;
    vm.targetMachinePCAfterCatch = dispatchAndCatchRoutine;
    vm.targetInterpreterPCForThrow = catchPCForInterpreter;
    vm.targetInterpreterMetadataPCForThrow = catchMetadataPCForInterpreter;
    vm.targetTryDepthForThrow = tryDepthForThrow;
    
    RELEASE_ASSERT(catchRoutine);
}

} // namespace JSC
