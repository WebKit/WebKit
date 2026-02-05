/*
 * Copyright (C) 2016 Yusuke Suzuki <utatane.tea@gmail.com>
 * Copyright (C) 2016-2023 Apple Inc. All rights reserved.
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

#include "CachedCall.h"
#include "Exception.h"
#include "FrameTracers.h"
#include "FunctionCodeBlock.h"
#include "FunctionExecutable.h"
#include "Instruction.h"
#include "Interpreter.h"
#include "JSCPtrTag.h"
#include "LLIntData.h"
#include "LLIntThunks.h"
#include "ProtoCallFrameInlines.h"
#include "StackAlignment.h"
#include "StackVisitor.h"
#include "UnlinkedCodeBlock.h"
#include "VMTrapsInlines.h"
#include <wtf/UnalignedAccess.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

ALWAYS_INLINE VM& Interpreter::vm()
{
    return *std::bit_cast<VM*>(std::bit_cast<uint8_t*>(this) - OBJECT_OFFSETOF(VM, interpreter));
}

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

inline JSC::Opcode Interpreter::getOpcode(OpcodeID id)
{
    return LLInt::getOpcode(id);
}

// This function is only available as a debugging tool for development work.
// It is not currently used except in a RELEASE_ASSERT to ensure that it is
// working properly.
inline OpcodeID Interpreter::getOpcodeID(JSC::Opcode opcode)
{
#if ENABLE(COMPUTED_GOTO_OPCODES)
    ASSERT(isOpcode(opcode));
#if ENABLE(LLINT_EMBEDDED_OPCODE_ID)
    // The OpcodeID is embedded in the int32_t word preceding the location of
    // the LLInt code for the opcode (see the EMBED_OPCODE_ID_IF_NEEDED macro
    // in LowLevelInterpreter.cpp).
    const void* opcodeAddress = removeCodePtrTag(std::bit_cast<const void*>(opcode));
    const int32_t* opcodeIDAddress = std::bit_cast<int32_t*>(opcodeAddress) - 1;
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

ALWAYS_INLINE JSValue Interpreter::executeCachedCall(CachedCall& cachedCall)
{
    VM& vm = this->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    ASSERT(!vm.isCollectorBusyOnCurrentThread());
    ASSERT(vm.currentThreadIsHoldingAPILock());

    StackStats::CheckPoint stackCheckPoint;

    auto clobberizeValidator = makeScopeExit([&] {
        vm.didEnterVM = true;
    });

    // We don't handle `NonDebuggerAsyncEvents` explicitly here. This is a JS function (since this is CachedCall),
    // so the called JS function always handles it.

    auto* entry = cachedCall.m_addressForCall;
    if (!entry) [[unlikely]] {
        DeferTraps deferTraps(vm); // We can't jettison this code if we're about to run it.
        cachedCall.relink();
        RETURN_IF_EXCEPTION(throwScope, throwScope.exception());
        entry = cachedCall.m_addressForCall;
    }

    // Execute the code:
    throwScope.release();
    return JSValue::decode(vmEntryToJavaScript(entry, &vm, &cachedCall.m_protoCallFrame));
}

#if CPU(ARM64) && CPU(ADDRESS64) && !ENABLE(C_LOOP)
template<typename... Args> requires (std::is_convertible_v<Args, JSValue> && ...)
ALWAYS_INLINE JSValue Interpreter::tryCallWithArguments(CachedCall& cachedCall, JSValue thisValue, Args... args)
{
    VM& vm = this->vm();
    static_assert(sizeof...(args) <= 6);

    ASSERT(!vm.isCollectorBusyOnCurrentThread());
    ASSERT(vm.currentThreadIsHoldingAPILock());

    StackStats::CheckPoint stackCheckPoint;

    auto clobberizeValidator = makeScopeExit([&] {
        vm.didEnterVM = true;
    });

    // We don't handle `NonDebuggerAsyncEvents` explicitly here. This is a JS function (since this is CachedCall),
    // so the called JS function always handles it.

    auto* entry = cachedCall.m_addressForCall;
    if (!entry) [[unlikely]]
        return { };

    // Execute the code:
    auto* codeBlock = cachedCall.m_protoCallFrame.codeBlock();
    auto* callee = cachedCall.m_protoCallFrame.callee();

    if constexpr (!sizeof...(args))
        return JSValue::decode(vmEntryToJavaScriptWith0Arguments(entry, &vm, codeBlock, callee, thisValue, nullptr, args...));
    else if constexpr (sizeof...(args) == 1)
        return JSValue::decode(vmEntryToJavaScriptWith1Arguments(entry, &vm, codeBlock, callee, thisValue, nullptr, args...));
    else if constexpr (sizeof...(args) == 2)
        return JSValue::decode(vmEntryToJavaScriptWith2Arguments(entry, &vm, codeBlock, callee, thisValue, nullptr, args...));
    else if constexpr (sizeof...(args) == 3)
        return JSValue::decode(vmEntryToJavaScriptWith3Arguments(entry, &vm, codeBlock, callee, thisValue, nullptr, args...));
    else if constexpr (sizeof...(args) == 4)
        return JSValue::decode(vmEntryToJavaScriptWith4Arguments(entry, &vm, codeBlock, callee, thisValue, nullptr, args...));
    else if constexpr (sizeof...(args) == 5)
        return JSValue::decode(vmEntryToJavaScriptWith5Arguments(entry, &vm, codeBlock, callee, thisValue, nullptr, args...));
    else if constexpr (sizeof...(args) == 6)
        return JSValue::decode(vmEntryToJavaScriptWith6Arguments(entry, &vm, codeBlock, callee, thisValue, nullptr, args...));
    else
        return { };
}
#endif

inline void UnwindFunctorBase::copyCalleeSavesToEntryFrameCalleeSavesBuffer(StackVisitor& visitor) const
{
#if ENABLE(ASSEMBLER)
    CallFrame* callFrame = visitor->callFrame();
    std::optional<RegisterAtOffsetList> currentCalleeSaves = visitor->calleeSaveRegistersForUnwinding();

    if (!currentCalleeSaves)
        return;

    RegisterAtOffsetList* allCalleeSaves = RegisterSetBuilder::vmCalleeSaveRegisterOffsets();
    auto dontCopyRegisters = RegisterSetBuilder::stackRegisters();
    CPURegister* frame = reinterpret_cast<CPURegister*>(callFrame->registers());

    unsigned registerCount = currentCalleeSaves->registerCount();
    VMEntryRecord* record = vmEntryRecord(m_vm.topEntryFrame);
    for (unsigned i = 0; i < registerCount; i++) {
        RegisterAtOffset currentEntry = currentCalleeSaves->at(i);
        if (dontCopyRegisters.contains(currentEntry.reg(), IgnoreVectors))
            continue;
        RegisterAtOffset* calleeSavesEntry = allCalleeSaves->find(currentEntry.reg());

        if (!calleeSavesEntry) {
            if constexpr (!isARM_THUMB2())
                RELEASE_ASSERT_NOT_REACHED();
            // This can happen on ARMv7, because there are more callee save
            // registers in the system convention than in the VM convention,
            // so frames generated by Air callees might restore any system
            // callee-save registers and we don't know the correct offset to
            // restore them to in the destination record if the register is
            // not callee-save in the VM convention.

            // Luckily, it is correct for us to drop these--since the
            // Air-generated callee is only expected to preserve the VM
            // callee registers (when called from the VM), it doesn't need
            // to appear to preserve the non-VM-callee-saves if we unwind
            // its frame.
            continue;
        }
WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
        record->calleeSaveRegistersBuffer[calleeSavesEntry->offsetAsIndex()] = *(frame + currentEntry.offsetAsIndex());
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
    }
#else
    UNUSED_PARAM(visitor);
#endif
}

ALWAYS_INLINE void UnwindFunctorBase::notifyDebuggerOfUnwinding(JSGlobalObject* globalObject, CallFrame* callFrame) const
{
    Debugger* debugger = globalObject->debugger();
    if (!debugger) [[likely]]
        return;

    DeferTermination deferScope(m_vm);
    auto catchScope = DECLARE_TOP_EXCEPTION_SCOPE(m_vm);

    SuspendExceptionScope scope(m_vm);
    if (callFrame->isNativeCalleeFrame()
        || (callFrame->callee().isCell() && callFrame->callee().asCell()->inherits<JSFunction>()))
        debugger->unwindEvent(callFrame);
    else
        debugger->didExecuteProgram(callFrame);
    catchScope.assertNoException();
}

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
