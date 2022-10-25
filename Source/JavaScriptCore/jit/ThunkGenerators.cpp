/*
 * Copyright (C) 2010-2022 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ThunkGenerators.h"

#include "JITOperations.h"
#include "JITThunks.h"
#include "JSBoundFunction.h"
#include "JSRemoteFunction.h"
#include "LLIntThunks.h"
#include "MaxFrameExtentForSlowPathCall.h"
#include "SpecializedThunkJIT.h"
#include "ThunkGenerator.h"
#include <wtf/InlineASM.h>
#include <wtf/StdIntExtras.h>
#include <wtf/StringPrintStream.h>
#include <wtf/text/StringImpl.h>

#if ENABLE(JIT)

namespace JSC {

MacroAssemblerCodeRef<JITThunkPtrTag> handleExceptionGenerator(VM& vm)
{
    CCallHelpers jit;

    jit.copyCalleeSavesToEntryFrameCalleeSavesBuffer(vm.topEntryFrame, GPRInfo::argumentGPR0);

    jit.move(CCallHelpers::TrustedImmPtr(&vm), GPRInfo::argumentGPR0);
    jit.prepareCallOperation(vm);
    CCallHelpers::Call operation = jit.call(OperationPtrTag);
    jit.jumpToExceptionHandler(vm);

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::ExtraCTIThunk);
    patchBuffer.link<OperationPtrTag>(operation, operationLookupExceptionHandler);
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "handleException");
}

MacroAssemblerCodeRef<JITThunkPtrTag> handleExceptionWithCallFrameRollbackGenerator(VM& vm)
{
    CCallHelpers jit;

    jit.copyCalleeSavesToEntryFrameCalleeSavesBuffer(vm.topEntryFrame, GPRInfo::argumentGPR0);

    jit.move(CCallHelpers::TrustedImmPtr(&vm), GPRInfo::argumentGPR0);
    jit.prepareCallOperation(vm);
    CCallHelpers::Call operation = jit.call(OperationPtrTag);
    jit.jumpToExceptionHandler(vm);

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::ExtraCTIThunk);
    patchBuffer.link<OperationPtrTag>(operation, operationLookupExceptionHandlerFromCallerFrame);
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "handleExceptionWithCallFrameRollback");
}

MacroAssemblerCodeRef<JITThunkPtrTag> popThunkStackPreservesAndHandleExceptionGenerator(VM& vm)
{
    CCallHelpers jit;

    jit.emitCTIThunkEpilogue();
#if CPU(X86_64) // On the x86, emitCTIThunkEpilogue leaves the return PC on the stack. Drop it.
    jit.addPtr(CCallHelpers::TrustedImm32(sizeof(CPURegister)), X86Registers::esp);
#endif

    CCallHelpers::Jump continuation = jit.jump();

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::ExtraCTIThunk);
    auto handler = vm.getCTIStub(handleExceptionGenerator);
    patchBuffer.link(continuation, CodeLocationLabel(handler.retaggedCode<NoPtrTag>()));
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "popThunkStackPreservesAndHandleException");
}

MacroAssemblerCodeRef<JITThunkPtrTag> checkExceptionGenerator(VM& vm)
{
    CCallHelpers jit;

    // This thunk is tail called from other thunks, and the return address is always already tagged

    // Exception fuzzing can call a runtime function. So, we need to preserve the return address here.
    if (UNLIKELY(Options::useExceptionFuzz()))
        jit.emitCTIThunkPrologue(/* returnAddressAlreadyTagged: */ true);

    CCallHelpers::Jump handleException = jit.emitNonPatchableExceptionCheck(vm);

    if (UNLIKELY(Options::useExceptionFuzz()))
        jit.emitCTIThunkEpilogue();
    jit.ret();

    auto handlerGenerator = Options::useExceptionFuzz() ? popThunkStackPreservesAndHandleExceptionGenerator : handleExceptionGenerator;
#if CPU(X86_64)
    if (!Options::useExceptionFuzz()) {
        handleException.link(&jit);
        jit.addPtr(CCallHelpers::TrustedImm32(sizeof(CPURegister)), X86Registers::esp); // pop return address.
        handleException = jit.jump();
    }
#endif

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::ExtraCTIThunk);
    patchBuffer.link(handleException, CodeLocationLabel(vm.getCTIStub(handlerGenerator).retaggedCode<NoPtrTag>()));
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "CheckException");
}

template<typename TagType>
inline void emitPointerValidation(CCallHelpers& jit, GPRReg pointerGPR, TagType tag)
{
#if CPU(ARM64E)
    if (!ASSERT_ENABLED)
        return;
    if (!Options::useJITCage()) {
        CCallHelpers::Jump isNonZero = jit.branchTestPtr(CCallHelpers::NonZero, pointerGPR);
        jit.abortWithReason(TGInvalidPointer);
        isNonZero.link(&jit);
        jit.pushToSave(pointerGPR);
        jit.untagPtr(tag, pointerGPR);
        jit.validateUntaggedPtr(pointerGPR);
        jit.popToRestore(pointerGPR);
    }
#else
    UNUSED_PARAM(jit);
    UNUSED_PARAM(pointerGPR);
    UNUSED_PARAM(tag);
#endif
}

// We will jump here if the JIT code tries to make a call, but the
// linking helper (C++ code) decides to throw an exception instead.
MacroAssemblerCodeRef<JITThunkPtrTag> throwExceptionFromCallSlowPathGenerator(VM& vm)
{
    CCallHelpers jit;
    
    // The call pushed a return address, so we need to pop it back off to re-align the stack,
    // even though we won't use it.
    jit.preserveReturnAddressAfterCall(GPRInfo::nonPreservedNonReturnGPR);

    jit.copyCalleeSavesToEntryFrameCalleeSavesBuffer(vm.topEntryFrame, GPRInfo::argumentGPR0);

    jit.setupArguments<decltype(operationLookupExceptionHandler)>(CCallHelpers::TrustedImmPtr(&vm));
    jit.prepareCallOperation(vm);
    jit.move(CCallHelpers::TrustedImmPtr(tagCFunction<OperationPtrTag>(operationLookupExceptionHandler)), GPRInfo::nonArgGPR0);
    emitPointerValidation(jit, GPRInfo::nonArgGPR0, OperationPtrTag);
    jit.call(GPRInfo::nonArgGPR0, OperationPtrTag);
    jit.jumpToExceptionHandler(vm);

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::Thunk);
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "Throw exception from call slow path thunk");
}

static void slowPathFor(CCallHelpers& jit, VM& vm, Sprt_JITOperation_EGCli slowPathFunction)
{
    jit.emitFunctionPrologue();
    jit.storePtr(GPRInfo::callFrameRegister, &vm.topCallFrame);
#if OS(WINDOWS) && CPU(X86_64)
    // Windows X86_64 needs some space pointed to by arg0 for return types larger than 64 bits.
    // Other argument values are shift by 1. Use space on the stack for our two return values.
    // Moving the stack down maxFrameExtentForSlowPathCall bytes gives us room for our 3 arguments
    // and space for the 16 byte return area.
    jit.addPtr(CCallHelpers::TrustedImm32(-static_cast<int32_t>(maxFrameExtentForSlowPathCall)), CCallHelpers::stackPointerRegister);
    static_assert(GPRInfo::regT2 != GPRInfo::argumentGPR0);
    static_assert(GPRInfo::regT3 != GPRInfo::argumentGPR0);
    jit.move(GPRInfo::regT2, GPRInfo::argumentGPR0);
    jit.move(GPRInfo::regT3, GPRInfo::argumentGPR2);
    jit.move(GPRInfo::argumentGPR0, GPRInfo::argumentGPR3);
    jit.addPtr(CCallHelpers::TrustedImm32(32), CCallHelpers::stackPointerRegister, GPRInfo::argumentGPR0);
    jit.move(GPRInfo::callFrameRegister, GPRInfo::argumentGPR1);
    jit.move(CCallHelpers::TrustedImmPtr(tagCFunction<OperationPtrTag>(slowPathFunction)), GPRInfo::nonArgGPR0);
    emitPointerValidation(jit, GPRInfo::nonArgGPR0, OperationPtrTag);
    jit.call(GPRInfo::nonArgGPR0, OperationPtrTag);
    jit.loadPtr(CCallHelpers::Address(GPRInfo::returnValueGPR, 8), GPRInfo::returnValueGPR2);
    jit.loadPtr(CCallHelpers::Address(GPRInfo::returnValueGPR), GPRInfo::returnValueGPR);
    jit.addPtr(CCallHelpers::TrustedImm32(maxFrameExtentForSlowPathCall), CCallHelpers::stackPointerRegister);
#else
    if (maxFrameExtentForSlowPathCall)
        jit.addPtr(CCallHelpers::TrustedImm32(-maxFrameExtentForSlowPathCall), CCallHelpers::stackPointerRegister);
    jit.setupArguments<decltype(slowPathFunction)>(GPRInfo::regT3, GPRInfo::regT2);
    jit.move(CCallHelpers::TrustedImmPtr(tagCFunction<OperationPtrTag>(slowPathFunction)), GPRInfo::nonArgGPR0);
    emitPointerValidation(jit, GPRInfo::nonArgGPR0, OperationPtrTag);
    jit.call(GPRInfo::nonArgGPR0, OperationPtrTag);
    if (maxFrameExtentForSlowPathCall)
        jit.addPtr(CCallHelpers::TrustedImm32(maxFrameExtentForSlowPathCall), CCallHelpers::stackPointerRegister);
#endif

    // This slow call will return the address of one of the following:
    // 1) Exception throwing thunk.
    // 2) Host call return value returner thingy.
    // 3) The function to call.
    // The second return value GPR will hold a non-zero value for tail calls.

    emitPointerValidation(jit, GPRInfo::returnValueGPR, JSEntryPtrTag);
    jit.emitFunctionEpilogue();
    jit.untagReturnAddress();

    RELEASE_ASSERT(reinterpret_cast<void*>(KeepTheFrame) == reinterpret_cast<void*>(0));
    CCallHelpers::Jump doNotTrash = jit.branchTestPtr(CCallHelpers::Zero, GPRInfo::returnValueGPR2);

    jit.preserveReturnAddressAfterCall(GPRInfo::nonPreservedNonReturnGPR);
    jit.prepareForTailCallSlow(GPRInfo::returnValueGPR);

    doNotTrash.link(&jit);
    jit.farJump(GPRInfo::returnValueGPR, JSEntryPtrTag);
}

MacroAssemblerCodeRef<JITThunkPtrTag> linkCallThunkGenerator(VM& vm)
{
    // The return address is on the stack or in the link register. We will hence
    // save the return address to the call frame while we make a C++ function call
    // to perform linking and lazy compilation if necessary. We expect the callee
    // to be in regT0/regT1 (payload/tag), the CallFrame to have already
    // been adjusted, and all other registers to be available for use.
    CCallHelpers jit;

    slowPathFor(jit, vm, operationLinkCall);

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::Thunk);
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "Link call slow path thunk");
}

// For closure optimizations, we only include calls, since if you're using closures for
// object construction then you're going to lose big time anyway.
MacroAssemblerCodeRef<JITThunkPtrTag> linkPolymorphicCallThunkGenerator(VM& vm)
{
    CCallHelpers jit;

    slowPathFor(jit, vm, operationLinkPolymorphicCall);

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::Thunk);
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "Link polymorphic call slow path thunk");
}

// FIXME: We should distinguish between a megamorphic virtual call vs. a slow
// path virtual call so that we can enable fast tail calls for megamorphic
// virtual calls by using the shuffler.
// https://bugs.webkit.org/show_bug.cgi?id=148831
static MacroAssemblerCodeRef<JITThunkPtrTag> virtualThunkFor(VM& vm, CallMode mode, CodeSpecializationKind kind)
{
    // The callee is in regT0 (for JSVALUE32_64, the tag is in regT1).
    // The return address is on the stack, or in the link register. We will hence
    // jump to the callee, or save the return address to the call frame while we
    // make a C++ function call to the appropriate JIT operation.

    CCallHelpers jit;

    bool isTailCall = mode == CallMode::Tail;

    CCallHelpers::JumpList slowCase;
    
    // This is a slow path execution, and regT2 contains the CallLinkInfo. Count the
    // slow path execution for the profiler.
    jit.add32(
        CCallHelpers::TrustedImm32(1),
        CCallHelpers::Address(GPRInfo::regT2, CallLinkInfo::offsetOfSlowPathCount()));

    // FIXME: we should have a story for eliminating these checks. In many cases,
    // the DFG knows that the value is definitely a cell, or definitely a function.
    
#if USE(JSVALUE64)
    if (isTailCall) {
        // Tail calls could have clobbered the GPRInfo::notCellMaskRegister because they
        // restore callee saved registers before getthing here. So, let's materialize
        // the NotCellMask in a temp register and use the temp instead.
        slowCase.append(jit.branchIfNotCell(GPRInfo::regT0, DoNotHaveTagRegisters));
    } else
        slowCase.append(jit.branchIfNotCell(GPRInfo::regT0));
#else
    slowCase.append(jit.branchIfNotCell(GPRInfo::regT1));
#endif
    auto notJSFunction = jit.branchIfNotFunction(GPRInfo::regT0);
    
    // Now we know we have a JSFunction.

    jit.loadPtr(CCallHelpers::Address(GPRInfo::regT0, JSFunction::offsetOfExecutableOrRareData()), GPRInfo::regT0);
    auto hasExecutable = jit.branchTestPtr(CCallHelpers::Zero, GPRInfo::regT0, CCallHelpers::TrustedImm32(JSFunction::rareDataTag));
    jit.loadPtr(CCallHelpers::Address(GPRInfo::regT0, FunctionRareData::offsetOfExecutable() - JSFunction::rareDataTag), GPRInfo::regT0);
    hasExecutable.link(&jit);
    jit.loadPtr(
        CCallHelpers::Address(GPRInfo::regT0, ExecutableBase::offsetOfJITCodeWithArityCheckFor(kind)),
        GPRInfo::regT4);
    slowCase.append(jit.branchTestPtr(CCallHelpers::Zero, GPRInfo::regT4));
    
    // Now we know that we have a CodeBlock, and we're committed to making a fast call.

    auto isNative = jit.branchIfNotType(GPRInfo::regT0, FunctionExecutableType);
    jit.loadPtr(
        CCallHelpers::Address(GPRInfo::regT0, FunctionExecutable::offsetOfCodeBlockFor(kind)),
        GPRInfo::regT5);

    // Make a tail call. This will return back to JIT code.
    emitPointerValidation(jit, GPRInfo::regT4, JSEntryPtrTag);
    if (isTailCall) {
        jit.preserveReturnAddressAfterCall(GPRInfo::regT0);
        jit.prepareForTailCallSlow(GPRInfo::regT4, GPRInfo::regT5);
    }
    jit.storePtr(GPRInfo::regT5, CCallHelpers::calleeFrameCodeBlockBeforeTailCall());
    jit.farJump(GPRInfo::regT4, JSEntryPtrTag);

    // NullSetterFunctionType does not get the fast path support. But it is OK since using NullSetterFunctionType is extremely rare.
    notJSFunction.link(&jit);
    slowCase.append(jit.branchIfNotType(GPRInfo::regT0, InternalFunctionType));
    void* executableAddress = vm.getCTIInternalFunctionTrampolineFor(kind).taggedPtr();
    jit.move(CCallHelpers::TrustedImmPtr(executableAddress), GPRInfo::regT4);

    isNative.link(&jit);
    emitPointerValidation(jit, GPRInfo::regT4, JSEntryPtrTag);
    if (isTailCall) {
        jit.preserveReturnAddressAfterCall(GPRInfo::regT0);
        jit.prepareForTailCallSlow(GPRInfo::regT4);
    }
    jit.farJump(GPRInfo::regT4, JSEntryPtrTag);

    slowCase.link(&jit);
    
    // Here we don't know anything, so revert to the full slow path.
    slowPathFor(jit, vm, operationVirtualCall);

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::VirtualThunk);
    return FINALIZE_THUNK(
        patchBuffer, JITThunkPtrTag,
        "Virtual %s slow path thunk",
        mode == CallMode::Regular ? "call" : mode == CallMode::Tail ? "tail call" : "construct");
}

static MacroAssemblerCodeRef<JITThunkPtrTag> virtualThunkForRegularCall(VM& vm)
{
    return virtualThunkFor(vm, CallMode::Regular, CodeForCall);
}

static MacroAssemblerCodeRef<JITThunkPtrTag> virtualThunkForTailCall(VM& vm)
{
    return virtualThunkFor(vm, CallMode::Tail, CodeForCall);
}

static MacroAssemblerCodeRef<JITThunkPtrTag> virtualThunkForConstructConstruct(VM& vm)
{
    return virtualThunkFor(vm, CallMode::Construct, CodeForConstruct);
}

MacroAssemblerCodeRef<JITStubRoutinePtrTag> virtualThunkFor(VM& vm, CallMode callMode)
{
    auto generator = [&] () -> ThunkGenerator {
        switch (callMode) {
        case CallMode::Regular:
            return virtualThunkForRegularCall;
        case CallMode::Tail:
            return virtualThunkForTailCall;
        case CallMode::Construct:
            return virtualThunkForConstructConstruct;
        }
        RELEASE_ASSERT_NOT_REACHED();
    };
    return vm.getCTIStub(generator()).retagged<JITStubRoutinePtrTag>();
}

enum ThunkEntryType { EnterViaCall, EnterViaJumpWithSavedTags, EnterViaJumpWithoutSavedTags };
enum class ThunkFunctionType { JSFunction, InternalFunction };
enum class IncludeDebuggerHook { No, Yes };

static MacroAssemblerCodeRef<JITThunkPtrTag> nativeForGenerator(VM& vm, ThunkFunctionType thunkFunctionType, CodeSpecializationKind kind, ThunkEntryType entryType = EnterViaCall, IncludeDebuggerHook includeDebuggerHook = IncludeDebuggerHook::No)
{
    // FIXME: This should be able to log ShadowChicken prologue packets.
    // https://bugs.webkit.org/show_bug.cgi?id=155689

    int executableOffsetToFunction = NativeExecutable::offsetOfNativeFunctionFor(kind);
    
    JSInterfaceJIT jit(&vm);

    switch (entryType) {
    case EnterViaCall:
        jit.emitFunctionPrologue();
        break;
    case EnterViaJumpWithSavedTags:
#if USE(JSVALUE64)
        // We're coming from a specialized thunk that has saved the prior tag registers' contents.
        // Restore them now.
        jit.popPair(JSInterfaceJIT::numberTagRegister, JSInterfaceJIT::notCellMaskRegister);
#endif
        break;
    case EnterViaJumpWithoutSavedTags:
        jit.move(JSInterfaceJIT::framePointerRegister, JSInterfaceJIT::stackPointerRegister);
        break;
    }

    jit.emitPutToCallFrameHeader(nullptr, CallFrameSlot::codeBlock);
    jit.storePtr(GPRInfo::callFrameRegister, &vm.topCallFrame);

    if (includeDebuggerHook == IncludeDebuggerHook::Yes) {
        jit.move(JSInterfaceJIT::framePointerRegister, GPRInfo::argumentGPR0);
        auto debuggerWillCallNativeExecutable = jit.call(OperationPtrTag);
        jit.addLinkTask([=] (LinkBuffer& linkBuffer) {
            linkBuffer.link<OperationPtrTag>(debuggerWillCallNativeExecutable, operationDebuggerWillCallNativeExecutable);
        });
    }

    // Host function signature: f(JSGlobalObject*, CallFrame*);
#if CPU(X86_64) && OS(WINDOWS)
    // Leave space for the callee parameter home addresses.
    // At this point the stack is aligned to 16 bytes, but if this changes at some point, we need to emit code to align it.
    jit.subPtr(CCallHelpers::TrustedImm32(4 * sizeof(int64_t)), CCallHelpers::stackPointerRegister);
#elif CPU(MIPS)
    // Allocate stack space for (unused) 16 bytes (8-byte aligned) for 4 arguments.
    jit.subPtr(CCallHelpers::TrustedImm32(16), CCallHelpers::stackPointerRegister);
#endif

    jit.move(GPRInfo::callFrameRegister, GPRInfo::argumentGPR1);
    jit.emitGetFromCallFrameHeaderPtr(CallFrameSlot::callee, GPRInfo::argumentGPR2);

    if (thunkFunctionType == ThunkFunctionType::JSFunction) {
        jit.loadPtr(CCallHelpers::Address(GPRInfo::argumentGPR2, JSFunction::offsetOfScopeChain()), GPRInfo::argumentGPR0);
        jit.loadPtr(CCallHelpers::Address(GPRInfo::argumentGPR2, JSFunction::offsetOfExecutableOrRareData()), GPRInfo::argumentGPR2);
        auto hasExecutable = jit.branchTestPtr(CCallHelpers::Zero, GPRInfo::argumentGPR2, CCallHelpers::TrustedImm32(JSFunction::rareDataTag));
        jit.loadPtr(CCallHelpers::Address(GPRInfo::argumentGPR2, FunctionRareData::offsetOfExecutable() - JSFunction::rareDataTag), GPRInfo::argumentGPR2);
        hasExecutable.link(&jit);
        if (Options::useJITCage()) {
            jit.loadPtr(CCallHelpers::Address(GPRInfo::argumentGPR2, executableOffsetToFunction), GPRInfo::argumentGPR2);
            auto operationCall = jit.call(OperationPtrTag);
            jit.addLinkTask([=] (LinkBuffer& linkBuffer) {
                linkBuffer.link<OperationPtrTag>(operationCall, vmEntryHostFunction);
            });
        } else
            jit.call(CCallHelpers::Address(GPRInfo::argumentGPR2, executableOffsetToFunction), HostFunctionPtrTag);
    } else {
        ASSERT(thunkFunctionType == ThunkFunctionType::InternalFunction);
        jit.loadPtr(CCallHelpers::Address(GPRInfo::argumentGPR2, InternalFunction::offsetOfGlobalObject()), GPRInfo::argumentGPR0);
        if (Options::useJITCage()) {
            jit.loadPtr(CCallHelpers::Address(GPRInfo::argumentGPR2, InternalFunction::offsetOfNativeFunctionFor(kind)), GPRInfo::argumentGPR2);
            auto operationCall = jit.call(OperationPtrTag);
            jit.addLinkTask([=] (LinkBuffer& linkBuffer) {
                linkBuffer.link<OperationPtrTag>(operationCall, vmEntryHostFunction);
            });
        } else
            jit.call(CCallHelpers::Address(GPRInfo::argumentGPR2, InternalFunction::offsetOfNativeFunctionFor(kind)), HostFunctionPtrTag);
    }

#if CPU(X86_64) && OS(WINDOWS)
    jit.addPtr(CCallHelpers::TrustedImm32(4 * sizeof(int64_t)), CCallHelpers::stackPointerRegister);
#elif CPU(MIPS)
    jit.addPtr(CCallHelpers::TrustedImm32(16), CCallHelpers::stackPointerRegister);
#endif

    // Check for an exception
#if USE(JSVALUE64)
    jit.loadPtr(vm.addressOfException(), JSInterfaceJIT::regT2);
    JSInterfaceJIT::Jump exceptionHandler = jit.branchTestPtr(JSInterfaceJIT::NonZero, JSInterfaceJIT::regT2);
#else
    JSInterfaceJIT::Jump exceptionHandler = jit.branch32(
        JSInterfaceJIT::NotEqual,
        JSInterfaceJIT::AbsoluteAddress(vm.addressOfException()),
        JSInterfaceJIT::TrustedImm32(0));
#endif

    jit.emitFunctionEpilogue();
    // Return.
    jit.ret();

    // Handle an exception
    exceptionHandler.link(&jit);

    jit.copyCalleeSavesToEntryFrameCalleeSavesBuffer(vm.topEntryFrame, GPRInfo::argumentGPR0);
    jit.storePtr(JSInterfaceJIT::callFrameRegister, &vm.topCallFrame);
#if OS(WINDOWS)
    // Allocate space on stack for the 4 parameter registers.
    jit.subPtr(JSInterfaceJIT::TrustedImm32(4 * sizeof(int64_t)), JSInterfaceJIT::stackPointerRegister);
#elif CPU(MIPS)
    // Allocate stack space for (unused) 16 bytes (8-byte aligned) for 4 arguments.
    jit.subPtr(CCallHelpers::TrustedImm32(16), CCallHelpers::stackPointerRegister);
#endif
    jit.move(CCallHelpers::TrustedImmPtr(&vm), JSInterfaceJIT::argumentGPR0);
    jit.move(JSInterfaceJIT::TrustedImmPtr(tagCFunction<OperationPtrTag>(operationVMHandleException)), JSInterfaceJIT::regT3);
    jit.call(JSInterfaceJIT::regT3, OperationPtrTag);
#if OS(WINDOWS)
    jit.addPtr(JSInterfaceJIT::TrustedImm32(4 * sizeof(int64_t)), JSInterfaceJIT::stackPointerRegister);
#elif CPU(MIPS)
    jit.addPtr(CCallHelpers::TrustedImm32(16), CCallHelpers::stackPointerRegister);
#endif

    jit.jumpToExceptionHandler(vm);

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::Thunk);
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "%s %s%s%s trampoline", thunkFunctionType == ThunkFunctionType::JSFunction ? "native" : "internal", entryType == EnterViaJumpWithSavedTags ? "Tail With Saved Tags " : entryType == EnterViaJumpWithoutSavedTags ? "Tail Without Saved Tags " : "", toCString(kind).data(), includeDebuggerHook == IncludeDebuggerHook::Yes ? " Debugger" : "");
}

MacroAssemblerCodeRef<JITThunkPtrTag> nativeCallGenerator(VM& vm)
{
    return nativeForGenerator(vm, ThunkFunctionType::JSFunction, CodeForCall);
}

MacroAssemblerCodeRef<JITThunkPtrTag> nativeCallWithDebuggerHookGenerator(VM& vm)
{
    return nativeForGenerator(vm, ThunkFunctionType::JSFunction, CodeForCall, EnterViaCall, IncludeDebuggerHook::Yes);
}

MacroAssemblerCodeRef<JITThunkPtrTag> nativeTailCallGenerator(VM& vm)
{
    return nativeForGenerator(vm, ThunkFunctionType::JSFunction, CodeForCall, EnterViaJumpWithSavedTags);
}

MacroAssemblerCodeRef<JITThunkPtrTag> nativeTailCallWithoutSavedTagsGenerator(VM& vm)
{
    return nativeForGenerator(vm, ThunkFunctionType::JSFunction, CodeForCall, EnterViaJumpWithoutSavedTags);
}

MacroAssemblerCodeRef<JITThunkPtrTag> nativeConstructGenerator(VM& vm)
{
    return nativeForGenerator(vm, ThunkFunctionType::JSFunction, CodeForConstruct);
}

MacroAssemblerCodeRef<JITThunkPtrTag> nativeConstructWithDebuggerHookGenerator(VM& vm)
{
    return nativeForGenerator(vm, ThunkFunctionType::JSFunction, CodeForConstruct, EnterViaCall, IncludeDebuggerHook::Yes);
}

MacroAssemblerCodeRef<JITThunkPtrTag> internalFunctionCallGenerator(VM& vm)
{
    return nativeForGenerator(vm, ThunkFunctionType::InternalFunction, CodeForCall);
}

MacroAssemblerCodeRef<JITThunkPtrTag> internalFunctionConstructGenerator(VM& vm)
{
    return nativeForGenerator(vm, ThunkFunctionType::InternalFunction, CodeForConstruct);
}

MacroAssemblerCodeRef<JITThunkPtrTag> arityFixupGenerator(VM& vm)
{
    JSInterfaceJIT jit(&vm);

    // We enter with fixup count in argumentGPR0
    // We have the guarantee that a0, a1, a2, t3, t4 and t5 (or t0 for Windows) are all distinct :-)
#if USE(JSVALUE64)
#if OS(WINDOWS)
    const GPRReg extraTemp = JSInterfaceJIT::regT0;
#else
    const GPRReg extraTemp = JSInterfaceJIT::regT5;
#endif
    static_assert(noOverlap(GPRInfo::argumentGPR0, GPRInfo::argumentGPR1, GPRInfo::argumentGPR2, GPRInfo::regT3, GPRInfo::regT4, extraTemp));
#if CPU(X86_64)
    jit.pop(JSInterfaceJIT::regT4);
#endif
    jit.tagReturnAddress();
#if CPU(ARM64E)
    jit.loadPtr(JSInterfaceJIT::Address(GPRInfo::callFrameRegister, CallFrame::returnPCOffset()), GPRInfo::regT3);
    jit.addPtr(JSInterfaceJIT::TrustedImm32(sizeof(CallerFrameAndPC)), GPRInfo::callFrameRegister, extraTemp);
    jit.untagPtr(extraTemp, GPRInfo::regT3);
    jit.validateUntaggedPtr(GPRInfo::regT3, extraTemp);
    PtrTag tempReturnPCTag = static_cast<PtrTag>(random());
    jit.move(JSInterfaceJIT::TrustedImmPtr(tempReturnPCTag), extraTemp);
    jit.tagPtr(extraTemp, GPRInfo::regT3);
    jit.storePtr(GPRInfo::regT3, JSInterfaceJIT::Address(GPRInfo::callFrameRegister, CallFrame::returnPCOffset()));
#endif
    jit.move(JSInterfaceJIT::callFrameRegister, JSInterfaceJIT::regT3);
    jit.load32(JSInterfaceJIT::addressFor(CallFrameSlot::argumentCountIncludingThis), JSInterfaceJIT::argumentGPR2);
    jit.add32(JSInterfaceJIT::TrustedImm32(CallFrame::headerSizeInRegisters), JSInterfaceJIT::argumentGPR2);

    // Check to see if we have extra slots we can use
    //
    // argumentGPR0's padding is `align2(numParameters + CallFrame::headerSizeInRegisters) - (argumentCountIncludingThis + CallFrame::headerSizeInRegisters)`
    // And extra slot means the above padding is an odd number. And after filling extra slot, it becomes +1, thus even number.
    JSInterfaceJIT::Jump noExtraSlot = jit.branchTest32(MacroAssembler::Zero, JSInterfaceJIT::argumentGPR0, JSInterfaceJIT::TrustedImm32(stackAlignmentRegisters() - 1));
    jit.move(JSInterfaceJIT::TrustedImm64(JSValue::ValueUndefined), extraTemp);
    static_assert(stackAlignmentRegisters() == 2);
    jit.store64(extraTemp, MacroAssembler::BaseIndex(JSInterfaceJIT::callFrameRegister, JSInterfaceJIT::argumentGPR2, JSInterfaceJIT::TimesEight));
    jit.add32(JSInterfaceJIT::TrustedImm32(1), JSInterfaceJIT::argumentGPR2);
    jit.and32(JSInterfaceJIT::TrustedImm32(-stackAlignmentRegisters()), JSInterfaceJIT::argumentGPR0);
    JSInterfaceJIT::Jump done = jit.branchTest32(MacroAssembler::Zero, JSInterfaceJIT::argumentGPR0);
    noExtraSlot.link(&jit);

    // At this point, argumentGPR2 is `align2(argumentCountIncludingThis + CallFrame::headerSizeInRegisters)`,
    // and argumentGPR0 is `align2(numParameters + CallFrame::headerSizeInRegisters) - align2(argumentCountIncludingThis + CallFrame::headerSizeInRegisters)`
    // Thus both argumentGPR0 and argumentGPR2 are align2-ed.

    jit.neg64(JSInterfaceJIT::argumentGPR0);

    // Adjust call frame register and stack pointer to account for missing args.
    // We need to change the stack pointer first before performing copy/fill loops.
    // This stack space below the stack pointer is considered unused by OS. Therefore,
    // OS may corrupt this space when constructing a signal stack.
    jit.lshift64(JSInterfaceJIT::argumentGPR0, JSInterfaceJIT::TrustedImm32(3), extraTemp);
    jit.addPtr(extraTemp, JSInterfaceJIT::callFrameRegister);
    jit.untagReturnAddress();
    jit.addPtr(extraTemp, JSInterfaceJIT::stackPointerRegister);
    jit.tagReturnAddress();

    // Move current frame down argumentGPR0 number of slots
#if CPU(ARM64)
    jit.addPtr(JSInterfaceJIT::regT3, extraTemp);
    JSInterfaceJIT::Label copyLoop(jit.label());
    jit.loadPair64(CCallHelpers::PostIndexAddress(JSInterfaceJIT::regT3, 16), GPRInfo::argumentGPR1, GPRInfo::regT6);
    jit.storePair64(GPRInfo::argumentGPR1, GPRInfo::regT6, CCallHelpers::PostIndexAddress(extraTemp, 16));
    jit.branchSub32(MacroAssembler::NonZero, JSInterfaceJIT::TrustedImm32(2), JSInterfaceJIT::argumentGPR2).linkTo(copyLoop, &jit);

    // Fill in argumentGPR0 missing arg slots with undefined
    jit.move(JSInterfaceJIT::TrustedImm64(JSValue::ValueUndefined), GPRInfo::argumentGPR1);
    JSInterfaceJIT::Label fillUndefinedLoop(jit.label());
    jit.storePair64(GPRInfo::argumentGPR1, GPRInfo::argumentGPR1, CCallHelpers::PostIndexAddress(extraTemp, 16));
    jit.branchAdd32(MacroAssembler::NonZero, JSInterfaceJIT::TrustedImm32(2), JSInterfaceJIT::argumentGPR0).linkTo(fillUndefinedLoop, &jit);
#else
    JSInterfaceJIT::Label copyLoop(jit.label());
    jit.load64(CCallHelpers::Address(JSInterfaceJIT::regT3), extraTemp);
    jit.store64(extraTemp, MacroAssembler::BaseIndex(JSInterfaceJIT::regT3, JSInterfaceJIT::argumentGPR0, JSInterfaceJIT::TimesEight));
    jit.addPtr(JSInterfaceJIT::TrustedImm32(8), JSInterfaceJIT::regT3);
    jit.branchSub32(MacroAssembler::NonZero, JSInterfaceJIT::TrustedImm32(1), JSInterfaceJIT::argumentGPR2).linkTo(copyLoop, &jit);

    // Fill in argumentGPR0 missing arg slots with undefined
    jit.move(JSInterfaceJIT::argumentGPR0, JSInterfaceJIT::argumentGPR2);
    jit.move(JSInterfaceJIT::TrustedImm64(JSValue::ValueUndefined), extraTemp);
    JSInterfaceJIT::Label fillUndefinedLoop(jit.label());
    jit.store64(extraTemp, MacroAssembler::BaseIndex(JSInterfaceJIT::regT3, JSInterfaceJIT::argumentGPR0, JSInterfaceJIT::TimesEight));
    jit.addPtr(JSInterfaceJIT::TrustedImm32(8), JSInterfaceJIT::regT3);
    jit.branchAdd32(MacroAssembler::NonZero, JSInterfaceJIT::TrustedImm32(1), JSInterfaceJIT::argumentGPR2).linkTo(fillUndefinedLoop, &jit);
#endif

    done.link(&jit);

#if CPU(ARM64E)
    jit.loadPtr(JSInterfaceJIT::Address(GPRInfo::callFrameRegister, CallFrame::returnPCOffset()), GPRInfo::regT3);
    jit.move(JSInterfaceJIT::TrustedImmPtr(tempReturnPCTag), extraTemp);
    jit.untagPtr(extraTemp, GPRInfo::regT3);
    jit.validateUntaggedPtr(GPRInfo::regT3, extraTemp);
    jit.addPtr(JSInterfaceJIT::TrustedImm32(sizeof(CallerFrameAndPC)), GPRInfo::callFrameRegister, extraTemp);
    jit.tagPtr(extraTemp, GPRInfo::regT3);
    jit.storePtr(GPRInfo::regT3, JSInterfaceJIT::Address(GPRInfo::callFrameRegister, CallFrame::returnPCOffset()));
#endif

#if CPU(X86_64)
    jit.push(JSInterfaceJIT::regT4);
#endif
    jit.ret();
#else // USE(JSVALUE64) section above, USE(JSVALUE32_64) section below.
    jit.move(JSInterfaceJIT::callFrameRegister, JSInterfaceJIT::regT3);
    jit.load32(JSInterfaceJIT::addressFor(CallFrameSlot::argumentCountIncludingThis), JSInterfaceJIT::argumentGPR2);
    jit.add32(JSInterfaceJIT::TrustedImm32(CallFrame::headerSizeInRegisters), JSInterfaceJIT::argumentGPR2);

    // Check to see if we have extra slots we can use
    jit.move(JSInterfaceJIT::argumentGPR0, JSInterfaceJIT::argumentGPR1);
    jit.and32(JSInterfaceJIT::TrustedImm32(stackAlignmentRegisters() - 1), JSInterfaceJIT::argumentGPR1);
    JSInterfaceJIT::Jump noExtraSlot = jit.branchTest32(MacroAssembler::Zero, JSInterfaceJIT::argumentGPR1);
    JSInterfaceJIT::Label fillExtraSlots(jit.label());
    jit.move(JSInterfaceJIT::TrustedImm32(0), JSInterfaceJIT::regT5);
    jit.store32(JSInterfaceJIT::regT5, MacroAssembler::BaseIndex(JSInterfaceJIT::callFrameRegister, JSInterfaceJIT::argumentGPR2, JSInterfaceJIT::TimesEight, PayloadOffset));
    jit.move(JSInterfaceJIT::TrustedImm32(JSValue::UndefinedTag), JSInterfaceJIT::regT5);
    jit.store32(JSInterfaceJIT::regT5, MacroAssembler::BaseIndex(JSInterfaceJIT::callFrameRegister, JSInterfaceJIT::argumentGPR2, JSInterfaceJIT::TimesEight, TagOffset));
    jit.add32(JSInterfaceJIT::TrustedImm32(1), JSInterfaceJIT::argumentGPR2);
    jit.branchSub32(JSInterfaceJIT::NonZero, JSInterfaceJIT::TrustedImm32(1), JSInterfaceJIT::argumentGPR1).linkTo(fillExtraSlots, &jit);
    jit.and32(JSInterfaceJIT::TrustedImm32(-stackAlignmentRegisters()), JSInterfaceJIT::argumentGPR0);
    JSInterfaceJIT::Jump done = jit.branchTest32(MacroAssembler::Zero, JSInterfaceJIT::argumentGPR0);
    noExtraSlot.link(&jit);

    jit.neg32(JSInterfaceJIT::argumentGPR0);

    // Adjust call frame register and stack pointer to account for missing args.
    // We need to change the stack pointer first before performing copy/fill loops.
    // This stack space below the stack pointer is considered unused by OS. Therefore,
    // OS may corrupt this space when constructing a signal stack.
    jit.move(JSInterfaceJIT::argumentGPR0, JSInterfaceJIT::regT5);
    jit.lshift32(JSInterfaceJIT::TrustedImm32(3), JSInterfaceJIT::regT5);
    jit.addPtr(JSInterfaceJIT::regT5, JSInterfaceJIT::callFrameRegister);
    jit.untagReturnAddress();
    jit.addPtr(JSInterfaceJIT::regT5, JSInterfaceJIT::stackPointerRegister);
    jit.tagReturnAddress();

    // Move current frame down argumentGPR0 number of slots
    JSInterfaceJIT::Label copyLoop(jit.label());
    jit.load32(MacroAssembler::Address(JSInterfaceJIT::regT3, PayloadOffset), JSInterfaceJIT::regT5);
    jit.store32(JSInterfaceJIT::regT5, MacroAssembler::BaseIndex(JSInterfaceJIT::regT3, JSInterfaceJIT::argumentGPR0, JSInterfaceJIT::TimesEight, PayloadOffset));
    jit.load32(MacroAssembler::Address(JSInterfaceJIT::regT3, TagOffset), JSInterfaceJIT::regT5);
    jit.store32(JSInterfaceJIT::regT5, MacroAssembler::BaseIndex(JSInterfaceJIT::regT3, JSInterfaceJIT::argumentGPR0, JSInterfaceJIT::TimesEight, TagOffset));
    jit.addPtr(JSInterfaceJIT::TrustedImm32(8), JSInterfaceJIT::regT3);
    jit.branchSub32(MacroAssembler::NonZero, JSInterfaceJIT::TrustedImm32(1), JSInterfaceJIT::argumentGPR2).linkTo(copyLoop, &jit);

    // Fill in argumentGPR0 missing arg slots with undefined
    jit.move(JSInterfaceJIT::argumentGPR0, JSInterfaceJIT::argumentGPR2);
    JSInterfaceJIT::Label fillUndefinedLoop(jit.label());
    jit.move(JSInterfaceJIT::TrustedImm32(0), JSInterfaceJIT::regT5);
    jit.store32(JSInterfaceJIT::regT5, MacroAssembler::BaseIndex(JSInterfaceJIT::regT3, JSInterfaceJIT::argumentGPR0, JSInterfaceJIT::TimesEight, PayloadOffset));
    jit.move(JSInterfaceJIT::TrustedImm32(JSValue::UndefinedTag), JSInterfaceJIT::regT5);
    jit.store32(JSInterfaceJIT::regT5, MacroAssembler::BaseIndex(JSInterfaceJIT::regT3, JSInterfaceJIT::argumentGPR0, JSInterfaceJIT::TimesEight, TagOffset));

    jit.addPtr(JSInterfaceJIT::TrustedImm32(8), JSInterfaceJIT::regT3);
    jit.branchAdd32(MacroAssembler::NonZero, JSInterfaceJIT::TrustedImm32(1), JSInterfaceJIT::argumentGPR2).linkTo(fillUndefinedLoop, &jit);

    done.link(&jit);

    jit.ret();
#endif // End of USE(JSVALUE32_64) section.

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::Thunk);
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "fixup arity");
}

MacroAssemblerCodeRef<JITThunkPtrTag> unreachableGenerator(VM& vm)
{
    JSInterfaceJIT jit(&vm);

    jit.breakpoint();

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::Thunk);
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "unreachable thunk");
}

MacroAssemblerCodeRef<JITThunkPtrTag> stringGetByValGenerator(VM& vm)
{
    // regT0 is JSString*, and regT1 (64bit) or regT2 (32bit) is int index.
    // Return regT0 = result JSString* if succeeds. Otherwise, return regT0 = 0.
#if USE(JSVALUE64)
    GPRReg stringGPR = GPRInfo::regT0;
    GPRReg indexGPR = GPRInfo::regT1;
    GPRReg scratchGPR = GPRInfo::regT2;
#else
    GPRReg stringGPR = GPRInfo::regT0;
    GPRReg indexGPR = GPRInfo::regT2;
    GPRReg scratchGPR = GPRInfo::regT1;
#endif

    JSInterfaceJIT jit(&vm);
    JSInterfaceJIT::JumpList failures;
    jit.tagReturnAddress();

    // Load string length to regT2, and start the process of loading the data pointer into regT0
    jit.loadPtr(JSInterfaceJIT::Address(stringGPR, JSString::offsetOfValue()), stringGPR);
    failures.append(jit.branchIfRopeStringImpl(stringGPR));
    jit.load32(JSInterfaceJIT::Address(stringGPR, StringImpl::lengthMemoryOffset()), scratchGPR);

    // Do an unsigned compare to simultaneously filter negative indices as well as indices that are too large
    failures.append(jit.branch32(JSInterfaceJIT::AboveOrEqual, indexGPR, scratchGPR));

    // Load the character
    JSInterfaceJIT::JumpList cont8Bit;
    // Load the string flags
    jit.load32(JSInterfaceJIT::Address(stringGPR, StringImpl::flagsOffset()), scratchGPR);
    jit.loadPtr(JSInterfaceJIT::Address(stringGPR, StringImpl::dataOffset()), stringGPR);
    auto is16Bit = jit.branchTest32(JSInterfaceJIT::Zero, scratchGPR, JSInterfaceJIT::TrustedImm32(StringImpl::flagIs8Bit()));
    jit.load8(JSInterfaceJIT::BaseIndex(stringGPR, indexGPR, JSInterfaceJIT::TimesOne, 0), stringGPR);
    cont8Bit.append(jit.jump());
    is16Bit.link(&jit);
    jit.load16(JSInterfaceJIT::BaseIndex(stringGPR, indexGPR, JSInterfaceJIT::TimesTwo, 0), stringGPR);
    cont8Bit.link(&jit);

    failures.append(jit.branch32(JSInterfaceJIT::Above, stringGPR, JSInterfaceJIT::TrustedImm32(maxSingleCharacterString)));
    jit.move(JSInterfaceJIT::TrustedImmPtr(vm.smallStrings.singleCharacterStrings()), indexGPR);
    jit.loadPtr(JSInterfaceJIT::BaseIndex(indexGPR, stringGPR, JSInterfaceJIT::ScalePtr, 0), stringGPR);
    jit.ret();

    failures.link(&jit);
    jit.move(JSInterfaceJIT::TrustedImm32(0), stringGPR);
    jit.ret();

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::Thunk);
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "String get_by_val stub");
}

static void stringCharLoad(SpecializedThunkJIT& jit)
{
    // load string
    jit.loadJSStringArgument(SpecializedThunkJIT::ThisArgument, SpecializedThunkJIT::regT0);

    // Load string length to regT2, and start the process of loading the data pointer into regT0
    jit.loadPtr(MacroAssembler::Address(SpecializedThunkJIT::regT0, JSString::offsetOfValue()), SpecializedThunkJIT::regT0);
    jit.appendFailure(jit.branchIfRopeStringImpl(SpecializedThunkJIT::regT0));
    jit.load32(MacroAssembler::Address(SpecializedThunkJIT::regT0, StringImpl::lengthMemoryOffset()), SpecializedThunkJIT::regT2);

    // load index
    jit.loadInt32Argument(0, SpecializedThunkJIT::regT1); // regT1 contains the index

    // Do an unsigned compare to simultaneously filter negative indices as well as indices that are too large
    jit.appendFailure(jit.branch32(MacroAssembler::AboveOrEqual, SpecializedThunkJIT::regT1, SpecializedThunkJIT::regT2));

    // Load the character
    SpecializedThunkJIT::JumpList is16Bit;
    SpecializedThunkJIT::JumpList cont8Bit;
    // Load the string flags
    jit.load32(MacroAssembler::Address(SpecializedThunkJIT::regT0, StringImpl::flagsOffset()), SpecializedThunkJIT::regT2);
    jit.loadPtr(MacroAssembler::Address(SpecializedThunkJIT::regT0, StringImpl::dataOffset()), SpecializedThunkJIT::regT0);
    is16Bit.append(jit.branchTest32(MacroAssembler::Zero, SpecializedThunkJIT::regT2, MacroAssembler::TrustedImm32(StringImpl::flagIs8Bit())));
    jit.load8(MacroAssembler::BaseIndex(SpecializedThunkJIT::regT0, SpecializedThunkJIT::regT1, MacroAssembler::TimesOne, 0), SpecializedThunkJIT::regT0);
    cont8Bit.append(jit.jump());
    is16Bit.link(&jit);
    jit.load16(MacroAssembler::BaseIndex(SpecializedThunkJIT::regT0, SpecializedThunkJIT::regT1, MacroAssembler::TimesTwo, 0), SpecializedThunkJIT::regT0);
    cont8Bit.link(&jit);
}

static void charToString(SpecializedThunkJIT& jit, VM& vm, MacroAssembler::RegisterID src, MacroAssembler::RegisterID dst, MacroAssembler::RegisterID scratch)
{
    jit.appendFailure(jit.branch32(MacroAssembler::Above, src, MacroAssembler::TrustedImm32(maxSingleCharacterString)));
    jit.move(MacroAssembler::TrustedImmPtr(vm.smallStrings.singleCharacterStrings()), scratch);
    jit.loadPtr(MacroAssembler::BaseIndex(scratch, src, MacroAssembler::ScalePtr, 0), dst);
    jit.appendFailure(jit.branchTestPtr(MacroAssembler::Zero, dst));
}

MacroAssemblerCodeRef<JITThunkPtrTag> charCodeAtThunkGenerator(VM& vm)
{
    SpecializedThunkJIT jit(vm, 1);
    stringCharLoad(jit);
    jit.returnInt32(SpecializedThunkJIT::regT0);
    return jit.finalize(vm.jitStubs->ctiNativeTailCall(vm), "charCodeAt");
}

MacroAssemblerCodeRef<JITThunkPtrTag> charAtThunkGenerator(VM& vm)
{
    SpecializedThunkJIT jit(vm, 1);
    stringCharLoad(jit);
    charToString(jit, vm, SpecializedThunkJIT::regT0, SpecializedThunkJIT::regT0, SpecializedThunkJIT::regT1);
    jit.returnJSCell(SpecializedThunkJIT::regT0);
    return jit.finalize(vm.jitStubs->ctiNativeTailCall(vm), "charAt");
}

MacroAssemblerCodeRef<JITThunkPtrTag> fromCharCodeThunkGenerator(VM& vm)
{
    SpecializedThunkJIT jit(vm, 1);
    // load char code
    jit.loadInt32Argument(0, SpecializedThunkJIT::regT0);
    charToString(jit, vm, SpecializedThunkJIT::regT0, SpecializedThunkJIT::regT0, SpecializedThunkJIT::regT1);
    jit.returnJSCell(SpecializedThunkJIT::regT0);
    return jit.finalize(vm.jitStubs->ctiNativeTailCall(vm), "fromCharCode");
}

MacroAssemblerCodeRef<JITThunkPtrTag> stringPrototypeCodePointAtThunkGenerator(VM& vm)
{
    SpecializedThunkJIT jit(vm, 1);

    // load string
    jit.loadJSStringArgument(SpecializedThunkJIT::ThisArgument, GPRInfo::regT0);

    // Load string length to regT3, and start the process of loading the data pointer into regT2
    jit.loadPtr(CCallHelpers::Address(GPRInfo::regT0, JSString::offsetOfValue()), GPRInfo::regT0);
    jit.appendFailure(jit.branchIfRopeStringImpl(GPRInfo::regT0));
    jit.load32(CCallHelpers::Address(GPRInfo::regT0, StringImpl::lengthMemoryOffset()), GPRInfo::regT3);

    // load index
    jit.loadInt32Argument(0, GPRInfo::regT1); // regT1 contains the index

    // Do an unsigned compare to simultaneously filter negative indices as well as indices that are too large
    jit.appendFailure(jit.branch32(CCallHelpers::AboveOrEqual, GPRInfo::regT1, GPRInfo::regT3));

    // Load the character
    CCallHelpers::JumpList done;
    // Load the string flags
    jit.loadPtr(CCallHelpers::Address(GPRInfo::regT0, StringImpl::dataOffset()), GPRInfo::regT2);
    auto is16Bit = jit.branchTest32(CCallHelpers::Zero, CCallHelpers::Address(GPRInfo::regT0, StringImpl::flagsOffset()), CCallHelpers::TrustedImm32(StringImpl::flagIs8Bit()));
    jit.load8(CCallHelpers::BaseIndex(GPRInfo::regT2, GPRInfo::regT1, CCallHelpers::TimesOne, 0), GPRInfo::regT0);
    done.append(jit.jump());

    is16Bit.link(&jit);
    jit.load16(CCallHelpers::BaseIndex(GPRInfo::regT2, GPRInfo::regT1, CCallHelpers::TimesTwo, 0), GPRInfo::regT0);
    // Original index is int32_t, and here, we ensure that it is positive. If we interpret it as uint32_t, adding 1 never overflows.
    jit.add32(CCallHelpers::TrustedImm32(1), GPRInfo::regT1);
    done.append(jit.branch32(CCallHelpers::AboveOrEqual, GPRInfo::regT1, GPRInfo::regT3));
    jit.and32(CCallHelpers::TrustedImm32(0xfffffc00), GPRInfo::regT0, GPRInfo::regT3);
    done.append(jit.branch32(CCallHelpers::NotEqual, GPRInfo::regT3, CCallHelpers::TrustedImm32(0xd800)));
    jit.load16(CCallHelpers::BaseIndex(GPRInfo::regT2, GPRInfo::regT1, CCallHelpers::TimesTwo, 0), GPRInfo::regT2);
    jit.and32(CCallHelpers::TrustedImm32(0xfffffc00), GPRInfo::regT2, GPRInfo::regT3);
    done.append(jit.branch32(CCallHelpers::NotEqual, GPRInfo::regT3, CCallHelpers::TrustedImm32(0xdc00)));
    jit.lshift32(CCallHelpers::TrustedImm32(10), GPRInfo::regT0);
    jit.getEffectiveAddress(CCallHelpers::BaseIndex(GPRInfo::regT0, GPRInfo::regT2, CCallHelpers::TimesOne, -U16_SURROGATE_OFFSET), GPRInfo::regT0);
    done.link(&jit);

    jit.returnInt32(GPRInfo::regT0);
    return jit.finalize(vm.jitStubs->ctiNativeTailCall(vm), "codePointAt");
}

MacroAssemblerCodeRef<JITThunkPtrTag> clz32ThunkGenerator(VM& vm)
{
    SpecializedThunkJIT jit(vm, 1);
    MacroAssembler::Jump nonIntArgJump;
    jit.loadInt32Argument(0, SpecializedThunkJIT::regT0, nonIntArgJump);

    SpecializedThunkJIT::Label convertedArgumentReentry(&jit);
    jit.countLeadingZeros32(SpecializedThunkJIT::regT0, SpecializedThunkJIT::regT1);
    jit.returnInt32(SpecializedThunkJIT::regT1);

    if (jit.supportsFloatingPointTruncate()) {
        nonIntArgJump.link(&jit);
        jit.loadDoubleArgument(0, SpecializedThunkJIT::fpRegT0, SpecializedThunkJIT::regT0);
        jit.branchTruncateDoubleToInt32(SpecializedThunkJIT::fpRegT0, SpecializedThunkJIT::regT0, SpecializedThunkJIT::BranchIfTruncateSuccessful).linkTo(convertedArgumentReentry, &jit);
        jit.appendFailure(jit.jump());
    } else
        jit.appendFailure(nonIntArgJump);

    return jit.finalize(vm.jitStubs->ctiNativeTailCall(vm), "clz32");
}

MacroAssemblerCodeRef<JITThunkPtrTag> sqrtThunkGenerator(VM& vm)
{
    SpecializedThunkJIT jit(vm, 1);
    if (!jit.supportsFloatingPointSqrt())
        return MacroAssemblerCodeRef<JITThunkPtrTag>::createSelfManagedCodeRef(vm.jitStubs->ctiNativeCall(vm));

    jit.loadDoubleArgument(0, SpecializedThunkJIT::fpRegT0, SpecializedThunkJIT::regT0);
    jit.sqrtDouble(SpecializedThunkJIT::fpRegT0, SpecializedThunkJIT::fpRegT0);
    jit.returnDouble(SpecializedThunkJIT::fpRegT0);
    return jit.finalize(vm.jitStubs->ctiNativeTailCall(vm), "sqrt");
}


#define UnaryDoubleOpWrapper(function) function##Wrapper
enum MathThunkCallingConvention { };
typedef MathThunkCallingConvention(*MathThunk)(MathThunkCallingConvention);

#if CPU(X86_64) && COMPILER(GCC_COMPATIBLE) && (OS(DARWIN) || OS(LINUX))

#define defineUnaryDoubleOpWrapper(function) \
    asm( \
        ".text\n" \
        ".globl " SYMBOL_STRING(function##Thunk) "\n" \
        HIDE_SYMBOL(function##Thunk) "\n" \
        SYMBOL_STRING(function##Thunk) ":" "\n" \
        "pushq %rax\n" \
        "call " GLOBAL_REFERENCE(function) "\n" \
        "popq %rcx\n" \
        "ret\n" \
        ".previous\n" \
    );\
    extern "C" { \
        MathThunkCallingConvention function##Thunk(MathThunkCallingConvention); \
        JSC_ANNOTATE_JIT_OPERATION(function##Thunk); \
    } \
    static MathThunk UnaryDoubleOpWrapper(function) = &function##Thunk;

#elif CPU(X86) && COMPILER(GCC_COMPATIBLE) && OS(LINUX) && defined(__PIC__)
#define defineUnaryDoubleOpWrapper(function) \
    asm( \
        ".text\n" \
        ".globl " SYMBOL_STRING(function##Thunk) "\n" \
        HIDE_SYMBOL(function##Thunk) "\n" \
        SYMBOL_STRING(function##Thunk) ":" "\n" \
        "pushl %ebx\n" \
        "subl $20, %esp\n" \
        "movsd %xmm0, (%esp) \n" \
        "call __x86.get_pc_thunk.bx\n" \
        "addl $_GLOBAL_OFFSET_TABLE_, %ebx\n" \
        "call " GLOBAL_REFERENCE(function) "\n" \
        "fstpl (%esp) \n" \
        "movsd (%esp), %xmm0 \n" \
        "addl $20, %esp\n" \
        "popl %ebx\n" \
        "ret\n" \
        ".previous\n" \
    );\
    extern "C" { \
        MathThunkCallingConvention function##Thunk(MathThunkCallingConvention); \
        JSC_ANNOTATE_JIT_OPERATION(function##Thunk); \
    } \
    static MathThunk UnaryDoubleOpWrapper(function) = &function##Thunk;

#elif CPU(X86) && COMPILER(GCC_COMPATIBLE) && (OS(DARWIN) || OS(LINUX))
#define defineUnaryDoubleOpWrapper(function) \
    asm( \
        ".text\n" \
        ".globl " SYMBOL_STRING(function##Thunk) "\n" \
        HIDE_SYMBOL(function##Thunk) "\n" \
        SYMBOL_STRING(function##Thunk) ":" "\n" \
        "subl $20, %esp\n" \
        "movsd %xmm0, (%esp) \n" \
        "call " GLOBAL_REFERENCE(function) "\n" \
        "fstpl (%esp) \n" \
        "movsd (%esp), %xmm0 \n" \
        "addl $20, %esp\n" \
        "ret\n" \
        ".previous\n" \
    );\
    extern "C" { \
        MathThunkCallingConvention function##Thunk(MathThunkCallingConvention); \
        JSC_ANNOTATE_JIT_OPERATION(function##Thunk); \
    } \
    static MathThunk UnaryDoubleOpWrapper(function) = &function##Thunk;

#elif CPU(ARM_THUMB2) && COMPILER(GCC_COMPATIBLE) && OS(DARWIN)

#define defineUnaryDoubleOpWrapper(function) \
    asm( \
        ".text\n" \
        ".align 2\n" \
        ".globl " SYMBOL_STRING(function##Thunk) "\n" \
        HIDE_SYMBOL(function##Thunk) "\n" \
        ".thumb\n" \
        ".thumb_func " THUMB_FUNC_PARAM(function##Thunk) "\n" \
        SYMBOL_STRING(function##Thunk) ":" "\n" \
        "push {lr}\n" \
        "vmov r0, r1, d0\n" \
        "blx " GLOBAL_REFERENCE(function) "\n" \
        "vmov d0, r0, r1\n" \
        "pop {lr}\n" \
        "bx lr\n" \
        ".previous\n" \
    ); \
    extern "C" { \
        MathThunkCallingConvention function##Thunk(MathThunkCallingConvention); \
        JSC_ANNOTATE_JIT_OPERATION(function##Thunk); \
    } \
    static MathThunk UnaryDoubleOpWrapper(function) = &function##Thunk;

#elif CPU(ARM64)

#define defineUnaryDoubleOpWrapper(function) \
    asm( \
        ".text\n" \
        ".align 2\n" \
        ".globl " SYMBOL_STRING(function##Thunk) "\n" \
        HIDE_SYMBOL(function##Thunk) "\n" \
        SYMBOL_STRING(function##Thunk) ":" "\n" \
        "b " GLOBAL_REFERENCE(function) "\n" \
        ".previous\n" \
    ); \
    extern "C" { \
        MathThunkCallingConvention function##Thunk(MathThunkCallingConvention); \
        JSC_ANNOTATE_JIT_OPERATION(function##Thunk); \
    } \
    static MathThunk UnaryDoubleOpWrapper(function) = &function##Thunk;

#elif CPU(X86) && COMPILER(MSVC) && OS(WINDOWS)

// MSVC does not accept floor, etc, to be called directly from inline assembly, so we need to wrap these functions.
static double (_cdecl *floorFunction)(double) = floor;
static double (_cdecl *ceilFunction)(double) = ceil;
static double (_cdecl *truncFunction)(double) = trunc;
static double (_cdecl *expFunction)(double) = exp;
static double (_cdecl *logFunction)(double) = log;
static double (_cdecl *jsRoundFunction)(double) = jsRound;

#define defineUnaryDoubleOpWrapper(function) \
    extern "C" __declspec(naked) MathThunkCallingConvention function##Thunk(MathThunkCallingConvention) \
    { \
        __asm \
        { \
        __asm sub esp, 20 \
        __asm movsd mmword ptr [esp], xmm0  \
        __asm call function##Function \
        __asm fstp qword ptr [esp] \
        __asm movsd xmm0, mmword ptr [esp] \
        __asm add esp, 20 \
        __asm ret \
        } \
    } \
    JSC_ANNOTATE_JIT_OPERATION(function##Thunk); \
    static MathThunk UnaryDoubleOpWrapper(function) = &function##Thunk;

#else

#define defineUnaryDoubleOpWrapper(function) \
    static MathThunk UnaryDoubleOpWrapper(function) = 0
#endif

defineUnaryDoubleOpWrapper(jsRound);
defineUnaryDoubleOpWrapper(exp);
defineUnaryDoubleOpWrapper(log);
defineUnaryDoubleOpWrapper(floor);
defineUnaryDoubleOpWrapper(ceil);
defineUnaryDoubleOpWrapper(trunc);
    
MacroAssemblerCodeRef<JITThunkPtrTag> floorThunkGenerator(VM& vm)
{
    SpecializedThunkJIT jit(vm, 1);
    MacroAssembler::Jump nonIntJump;
    if (!UnaryDoubleOpWrapper(floor) || !jit.supportsFloatingPoint())
        return MacroAssemblerCodeRef<JITThunkPtrTag>::createSelfManagedCodeRef(vm.jitStubs->ctiNativeCall(vm));
    jit.loadInt32Argument(0, SpecializedThunkJIT::regT0, nonIntJump);
    jit.returnInt32(SpecializedThunkJIT::regT0);
    nonIntJump.link(&jit);
    jit.loadDoubleArgument(0, SpecializedThunkJIT::fpRegT0, SpecializedThunkJIT::regT0);

    if (jit.supportsFloatingPointRounding()) {
        SpecializedThunkJIT::JumpList doubleResult;
        jit.floorDouble(SpecializedThunkJIT::fpRegT0, SpecializedThunkJIT::fpRegT0);
        jit.branchConvertDoubleToInt32(SpecializedThunkJIT::fpRegT0, SpecializedThunkJIT::regT0, doubleResult, SpecializedThunkJIT::fpRegT1);
        jit.returnInt32(SpecializedThunkJIT::regT0);
        doubleResult.link(&jit);
        jit.returnDouble(SpecializedThunkJIT::fpRegT0);
        return jit.finalize(vm.jitStubs->ctiNativeTailCall(vm), "floor");
    }

    SpecializedThunkJIT::Jump intResult;
    SpecializedThunkJIT::JumpList doubleResult;
    if (jit.supportsFloatingPointTruncate()) {
        jit.moveZeroToDouble(SpecializedThunkJIT::fpRegT1);
        doubleResult.append(jit.branchDouble(MacroAssembler::DoubleEqualAndOrdered, SpecializedThunkJIT::fpRegT0, SpecializedThunkJIT::fpRegT1));
        SpecializedThunkJIT::JumpList slowPath;
        // Handle the negative doubles in the slow path for now.
        slowPath.append(jit.branchDouble(MacroAssembler::DoubleLessThanOrUnordered, SpecializedThunkJIT::fpRegT0, SpecializedThunkJIT::fpRegT1));
        slowPath.append(jit.branchTruncateDoubleToInt32(SpecializedThunkJIT::fpRegT0, SpecializedThunkJIT::regT0));
        intResult = jit.jump();
        slowPath.link(&jit);
    }
    jit.callDoubleToDoublePreservingReturn(UnaryDoubleOpWrapper(floor));
    jit.branchConvertDoubleToInt32(SpecializedThunkJIT::fpRegT0, SpecializedThunkJIT::regT0, doubleResult, SpecializedThunkJIT::fpRegT1);
    if (jit.supportsFloatingPointTruncate())
        intResult.link(&jit);
    jit.returnInt32(SpecializedThunkJIT::regT0);
    doubleResult.link(&jit);
    jit.returnDouble(SpecializedThunkJIT::fpRegT0);
    return jit.finalize(vm.jitStubs->ctiNativeTailCall(vm), "floor");
}

MacroAssemblerCodeRef<JITThunkPtrTag> ceilThunkGenerator(VM& vm)
{
    SpecializedThunkJIT jit(vm, 1);
    if (!UnaryDoubleOpWrapper(ceil) || !jit.supportsFloatingPoint())
        return MacroAssemblerCodeRef<JITThunkPtrTag>::createSelfManagedCodeRef(vm.jitStubs->ctiNativeCall(vm));
    MacroAssembler::Jump nonIntJump;
    jit.loadInt32Argument(0, SpecializedThunkJIT::regT0, nonIntJump);
    jit.returnInt32(SpecializedThunkJIT::regT0);
    nonIntJump.link(&jit);
    jit.loadDoubleArgument(0, SpecializedThunkJIT::fpRegT0, SpecializedThunkJIT::regT0);
    if (jit.supportsFloatingPointRounding())
        jit.ceilDouble(SpecializedThunkJIT::fpRegT0, SpecializedThunkJIT::fpRegT0);
    else
        jit.callDoubleToDoublePreservingReturn(UnaryDoubleOpWrapper(ceil));

    SpecializedThunkJIT::JumpList doubleResult;
    jit.branchConvertDoubleToInt32(SpecializedThunkJIT::fpRegT0, SpecializedThunkJIT::regT0, doubleResult, SpecializedThunkJIT::fpRegT1);
    jit.returnInt32(SpecializedThunkJIT::regT0);
    doubleResult.link(&jit);
    jit.returnDouble(SpecializedThunkJIT::fpRegT0);
    return jit.finalize(vm.jitStubs->ctiNativeTailCall(vm), "ceil");
}

MacroAssemblerCodeRef<JITThunkPtrTag> truncThunkGenerator(VM& vm)
{
    SpecializedThunkJIT jit(vm, 1);
    if (!UnaryDoubleOpWrapper(trunc) || !jit.supportsFloatingPoint())
        return MacroAssemblerCodeRef<JITThunkPtrTag>::createSelfManagedCodeRef(vm.jitStubs->ctiNativeCall(vm));
    MacroAssembler::Jump nonIntJump;
    jit.loadInt32Argument(0, SpecializedThunkJIT::regT0, nonIntJump);
    jit.returnInt32(SpecializedThunkJIT::regT0);
    nonIntJump.link(&jit);
    jit.loadDoubleArgument(0, SpecializedThunkJIT::fpRegT0, SpecializedThunkJIT::regT0);
    if (jit.supportsFloatingPointRounding())
        jit.roundTowardZeroDouble(SpecializedThunkJIT::fpRegT0, SpecializedThunkJIT::fpRegT0);
    else
        jit.callDoubleToDoublePreservingReturn(UnaryDoubleOpWrapper(trunc));

    SpecializedThunkJIT::JumpList doubleResult;
    jit.branchConvertDoubleToInt32(SpecializedThunkJIT::fpRegT0, SpecializedThunkJIT::regT0, doubleResult, SpecializedThunkJIT::fpRegT1);
    jit.returnInt32(SpecializedThunkJIT::regT0);
    doubleResult.link(&jit);
    jit.returnDouble(SpecializedThunkJIT::fpRegT0);
    return jit.finalize(vm.jitStubs->ctiNativeTailCall(vm), "trunc");
}

MacroAssemblerCodeRef<JITThunkPtrTag> roundThunkGenerator(VM& vm)
{
    SpecializedThunkJIT jit(vm, 1);
    if (!UnaryDoubleOpWrapper(jsRound) || !jit.supportsFloatingPoint())
        return MacroAssemblerCodeRef<JITThunkPtrTag>::createSelfManagedCodeRef(vm.jitStubs->ctiNativeCall(vm));
    MacroAssembler::Jump nonIntJump;
    jit.loadInt32Argument(0, SpecializedThunkJIT::regT0, nonIntJump);
    jit.returnInt32(SpecializedThunkJIT::regT0);
    nonIntJump.link(&jit);
    jit.loadDoubleArgument(0, SpecializedThunkJIT::fpRegT0, SpecializedThunkJIT::regT0);
    SpecializedThunkJIT::JumpList doubleResult;
    if (jit.supportsFloatingPointRounding()) {
        jit.moveZeroToDouble(SpecializedThunkJIT::fpRegT1);
        doubleResult.append(jit.branchDouble(MacroAssembler::DoubleEqualAndOrdered, SpecializedThunkJIT::fpRegT0, SpecializedThunkJIT::fpRegT1));

        jit.ceilDouble(SpecializedThunkJIT::fpRegT0, SpecializedThunkJIT::fpRegT1);
        static constexpr double halfConstant = -0.5;
        jit.loadDouble(MacroAssembler::TrustedImmPtr(&halfConstant), SpecializedThunkJIT::fpRegT2);
        jit.addDouble(SpecializedThunkJIT::fpRegT1, SpecializedThunkJIT::fpRegT2);
        MacroAssembler::Jump shouldRoundDown = jit.branchDouble(MacroAssembler::DoubleGreaterThanAndOrdered, SpecializedThunkJIT::fpRegT2, SpecializedThunkJIT::fpRegT0);

        jit.moveDouble(SpecializedThunkJIT::fpRegT1, SpecializedThunkJIT::fpRegT0);
        MacroAssembler::Jump continuation = jit.jump();

        shouldRoundDown.link(&jit);
        static constexpr double oneConstant = 1.0;
        jit.loadDouble(MacroAssembler::TrustedImmPtr(&oneConstant), SpecializedThunkJIT::fpRegT2);
        jit.subDouble(SpecializedThunkJIT::fpRegT1, SpecializedThunkJIT::fpRegT2, SpecializedThunkJIT::fpRegT0);

        continuation.link(&jit);
    } else
        jit.callDoubleToDoublePreservingReturn(UnaryDoubleOpWrapper(jsRound));
    jit.branchConvertDoubleToInt32(SpecializedThunkJIT::fpRegT0, SpecializedThunkJIT::regT0, doubleResult, SpecializedThunkJIT::fpRegT1);
    jit.returnInt32(SpecializedThunkJIT::regT0);
    doubleResult.link(&jit);
    jit.returnDouble(SpecializedThunkJIT::fpRegT0);
    return jit.finalize(vm.jitStubs->ctiNativeTailCall(vm), "round");
}

MacroAssemblerCodeRef<JITThunkPtrTag> expThunkGenerator(VM& vm)
{
    if (!UnaryDoubleOpWrapper(exp))
        return MacroAssemblerCodeRef<JITThunkPtrTag>::createSelfManagedCodeRef(vm.jitStubs->ctiNativeCall(vm));
    SpecializedThunkJIT jit(vm, 1);
    if (!jit.supportsFloatingPoint())
        return MacroAssemblerCodeRef<JITThunkPtrTag>::createSelfManagedCodeRef(vm.jitStubs->ctiNativeCall(vm));
    jit.loadDoubleArgument(0, SpecializedThunkJIT::fpRegT0, SpecializedThunkJIT::regT0);
    jit.callDoubleToDoublePreservingReturn(UnaryDoubleOpWrapper(exp));
    jit.returnDouble(SpecializedThunkJIT::fpRegT0);
    return jit.finalize(vm.jitStubs->ctiNativeTailCall(vm), "exp");
}

MacroAssemblerCodeRef<JITThunkPtrTag> logThunkGenerator(VM& vm)
{
    if (!UnaryDoubleOpWrapper(log))
        return MacroAssemblerCodeRef<JITThunkPtrTag>::createSelfManagedCodeRef(vm.jitStubs->ctiNativeCall(vm));
    SpecializedThunkJIT jit(vm, 1);
    if (!jit.supportsFloatingPoint())
        return MacroAssemblerCodeRef<JITThunkPtrTag>::createSelfManagedCodeRef(vm.jitStubs->ctiNativeCall(vm));
    jit.loadDoubleArgument(0, SpecializedThunkJIT::fpRegT0, SpecializedThunkJIT::regT0);
    jit.callDoubleToDoublePreservingReturn(UnaryDoubleOpWrapper(log));
    jit.returnDouble(SpecializedThunkJIT::fpRegT0);
    return jit.finalize(vm.jitStubs->ctiNativeTailCall(vm), "log");
}

MacroAssemblerCodeRef<JITThunkPtrTag> absThunkGenerator(VM& vm)
{
    SpecializedThunkJIT jit(vm, 1);
    if (!jit.supportsFloatingPointAbs())
        return MacroAssemblerCodeRef<JITThunkPtrTag>::createSelfManagedCodeRef(vm.jitStubs->ctiNativeCall(vm));

#if USE(JSVALUE64)
    VirtualRegister virtualRegister = CallFrameSlot::firstArgument;
    jit.load64(AssemblyHelpers::addressFor(virtualRegister), GPRInfo::regT0);
    auto notInteger = jit.branchIfNotInt32(GPRInfo::regT0);

    // Abs Int32.
    jit.rshift32(GPRInfo::regT0, MacroAssembler::TrustedImm32(31), GPRInfo::regT1);
    jit.add32(GPRInfo::regT1, GPRInfo::regT0);
    jit.xor32(GPRInfo::regT1, GPRInfo::regT0);

    // IntMin cannot be inverted.
    MacroAssembler::Jump integerIsIntMin = jit.branchTest32(MacroAssembler::Signed, GPRInfo::regT0);

    // Box and finish.
    jit.or64(GPRInfo::numberTagRegister, GPRInfo::regT0);
    MacroAssembler::Jump doneWithIntegers = jit.jump();

    // Handle Doubles.
    notInteger.link(&jit);
    jit.appendFailure(jit.branchIfNotNumber(GPRInfo::regT0));
    jit.unboxDoubleWithoutAssertions(GPRInfo::regT0, GPRInfo::regT0, FPRInfo::fpRegT0);
    MacroAssembler::Label absFPR0Label = jit.label();
    jit.absDouble(FPRInfo::fpRegT0, FPRInfo::fpRegT1);
    jit.boxDouble(FPRInfo::fpRegT1, GPRInfo::regT0);

    // Tail.
    doneWithIntegers.link(&jit);
    jit.returnJSValue(GPRInfo::regT0);

    // We know the value of regT0 is IntMin. We could load that value from memory but
    // it is simpler to just convert it.
    integerIsIntMin.link(&jit);
    jit.convertInt32ToDouble(GPRInfo::regT0, FPRInfo::fpRegT0);
    jit.jump().linkTo(absFPR0Label, &jit);
#else
    MacroAssembler::Jump nonIntJump;
    jit.loadInt32Argument(0, SpecializedThunkJIT::regT0, nonIntJump);
    jit.rshift32(SpecializedThunkJIT::regT0, MacroAssembler::TrustedImm32(31), SpecializedThunkJIT::regT1);
    jit.add32(SpecializedThunkJIT::regT1, SpecializedThunkJIT::regT0);
    jit.xor32(SpecializedThunkJIT::regT1, SpecializedThunkJIT::regT0);
    jit.appendFailure(jit.branchTest32(MacroAssembler::Signed, SpecializedThunkJIT::regT0));
    jit.returnInt32(SpecializedThunkJIT::regT0);
    nonIntJump.link(&jit);
    // Shame about the double int conversion here.
    jit.loadDoubleArgument(0, SpecializedThunkJIT::fpRegT0, SpecializedThunkJIT::regT0);
    jit.absDouble(SpecializedThunkJIT::fpRegT0, SpecializedThunkJIT::fpRegT1);
    jit.returnDouble(SpecializedThunkJIT::fpRegT1);
#endif
    return jit.finalize(vm.jitStubs->ctiNativeTailCall(vm), "abs");
}

MacroAssemblerCodeRef<JITThunkPtrTag> imulThunkGenerator(VM& vm)
{
    SpecializedThunkJIT jit(vm, 2);
    MacroAssembler::Jump nonIntArg0Jump;
    jit.loadInt32Argument(0, SpecializedThunkJIT::regT0, nonIntArg0Jump);
    SpecializedThunkJIT::Label doneLoadingArg0(&jit);
    MacroAssembler::Jump nonIntArg1Jump;
    jit.loadInt32Argument(1, SpecializedThunkJIT::regT1, nonIntArg1Jump);
    SpecializedThunkJIT::Label doneLoadingArg1(&jit);
    jit.mul32(SpecializedThunkJIT::regT1, SpecializedThunkJIT::regT0);
    jit.returnInt32(SpecializedThunkJIT::regT0);

    if (jit.supportsFloatingPointTruncate()) {
        nonIntArg0Jump.link(&jit);
        jit.loadDoubleArgument(0, SpecializedThunkJIT::fpRegT0, SpecializedThunkJIT::regT0);
        jit.branchTruncateDoubleToInt32(SpecializedThunkJIT::fpRegT0, SpecializedThunkJIT::regT0, SpecializedThunkJIT::BranchIfTruncateSuccessful).linkTo(doneLoadingArg0, &jit);
        jit.appendFailure(jit.jump());
    } else
        jit.appendFailure(nonIntArg0Jump);

    if (jit.supportsFloatingPointTruncate()) {
        nonIntArg1Jump.link(&jit);
        jit.loadDoubleArgument(1, SpecializedThunkJIT::fpRegT0, SpecializedThunkJIT::regT1);
        jit.branchTruncateDoubleToInt32(SpecializedThunkJIT::fpRegT0, SpecializedThunkJIT::regT1, SpecializedThunkJIT::BranchIfTruncateSuccessful).linkTo(doneLoadingArg1, &jit);
        jit.appendFailure(jit.jump());
    } else
        jit.appendFailure(nonIntArg1Jump);

    return jit.finalize(vm.jitStubs->ctiNativeTailCall(vm), "imul");
}

MacroAssemblerCodeRef<JITThunkPtrTag> randomThunkGenerator(VM& vm)
{
    SpecializedThunkJIT jit(vm, 0);
    if (!jit.supportsFloatingPoint())
        return MacroAssemblerCodeRef<JITThunkPtrTag>::createSelfManagedCodeRef(vm.jitStubs->ctiNativeCall(vm));

#if USE(JSVALUE64)
    jit.emitRandomThunk(vm, SpecializedThunkJIT::regT0, SpecializedThunkJIT::regT1, SpecializedThunkJIT::regT2, SpecializedThunkJIT::regT3, SpecializedThunkJIT::fpRegT0);
    jit.returnDouble(SpecializedThunkJIT::fpRegT0);

    return jit.finalize(vm.jitStubs->ctiNativeTailCall(vm), "random");
#else
    return MacroAssemblerCodeRef<JITThunkPtrTag>::createSelfManagedCodeRef(vm.jitStubs->ctiNativeCall(vm));
#endif
}

MacroAssemblerCodeRef<JITThunkPtrTag> boundFunctionCallGenerator(VM& vm)
{
    CCallHelpers jit;
    
    jit.emitFunctionPrologue();
    
    // Set up our call frame.
    jit.storePtr(CCallHelpers::TrustedImmPtr(nullptr), CCallHelpers::addressFor(CallFrameSlot::codeBlock));
    jit.store32(CCallHelpers::TrustedImm32(0), CCallHelpers::tagFor(CallFrameSlot::argumentCountIncludingThis));

    constexpr unsigned stackMisalignment = sizeof(CallerFrameAndPC) % stackAlignmentBytes();
    constexpr unsigned extraStackNeeded = stackMisalignment ? stackAlignmentBytes() - stackMisalignment : 0;
    
    // We need to forward all of the arguments that we were passed. We aren't allowed to do a tail
    // call here as far as I can tell. At least not so long as the generic path doesn't do a tail
    // call, since that would be way too weird.
    
    // The formula for the number of stack bytes needed given some number of parameters (including
    // this) is:
    //
    //     stackAlign((numParams + CallFrameHeaderSize) * sizeof(Register) - sizeof(CallerFrameAndPC))
    //
    // Probably we want to write this as:
    //
    //     stackAlign((numParams + (CallFrameHeaderSize - CallerFrameAndPCSize)) * sizeof(Register))
    //
    // That's really all there is to this. We have all the registers we need to do it.
    
    jit.loadCell(CCallHelpers::addressFor(CallFrameSlot::callee), GPRInfo::regT0);
    jit.loadPtr(CCallHelpers::Address(GPRInfo::regT0, JSBoundFunction::offsetOfBoundArgs()), GPRInfo::regT2);
    jit.load32(CCallHelpers::payloadFor(CallFrameSlot::argumentCountIncludingThis), GPRInfo::regT1);
    jit.move(GPRInfo::regT1, GPRInfo::regT3);
    auto noArgs = jit.branchTestPtr(CCallHelpers::Zero, GPRInfo::regT2);
    jit.load32(CCallHelpers::Address(GPRInfo::regT2, JSImmutableButterfly::offsetOfPublicLength()), GPRInfo::regT2);
    jit.add32(GPRInfo::regT2, GPRInfo::regT1);
    jit.sub32(CCallHelpers::TrustedImm32(1), GPRInfo::regT1);
    noArgs.link(&jit);
    jit.add32(CCallHelpers::TrustedImm32(CallFrame::headerSizeInRegisters - CallerFrameAndPC::sizeInRegisters), GPRInfo::regT1, GPRInfo::regT2);
    jit.lshift32(CCallHelpers::TrustedImm32(3), GPRInfo::regT2);
    jit.add32(CCallHelpers::TrustedImm32(stackAlignmentBytes() - 1), GPRInfo::regT2);
    jit.and32(CCallHelpers::TrustedImm32(-stackAlignmentBytes()), GPRInfo::regT2);
    
    if (extraStackNeeded)
        jit.add32(CCallHelpers::TrustedImm32(extraStackNeeded), GPRInfo::regT2);
    
    // At this point regT1 has the actual argument count, regT2 has the amount of stack we will need, and regT3 has the passed argument count.
    // Check to see if we have enough stack space.
    
    jit.negPtr(GPRInfo::regT2);
    jit.addPtr(CCallHelpers::stackPointerRegister, GPRInfo::regT2);
    CCallHelpers::Jump haveStackSpace = jit.branchPtr(CCallHelpers::BelowOrEqual, CCallHelpers::AbsoluteAddress(vm.addressOfSoftStackLimit()), GPRInfo::regT2);

    // Throw Stack Overflow exception
    jit.copyCalleeSavesToEntryFrameCalleeSavesBuffer(vm.topEntryFrame, GPRInfo::regT3);
    jit.loadPtr(CCallHelpers::Address(GPRInfo::regT0, JSBoundFunction::offsetOfScopeChain()), GPRInfo::regT3);
    jit.setupArguments<decltype(operationThrowStackOverflowErrorFromThunk)>(GPRInfo::regT3);
    jit.prepareCallOperation(vm);
    jit.move(CCallHelpers::TrustedImmPtr(tagCFunction<OperationPtrTag>(operationThrowStackOverflowErrorFromThunk)), GPRInfo::nonArgGPR0);
    emitPointerValidation(jit, GPRInfo::nonArgGPR0, OperationPtrTag);
    jit.call(GPRInfo::nonArgGPR0, OperationPtrTag);
    jit.jumpToExceptionHandler(vm);

    haveStackSpace.link(&jit);
    jit.move(GPRInfo::regT2, CCallHelpers::stackPointerRegister);

    // Do basic callee frame setup, including 'this'.
    
    jit.store32(GPRInfo::regT1, CCallHelpers::calleeFramePayloadSlot(CallFrameSlot::argumentCountIncludingThis));
    
    JSValueRegs valueRegs = JSValueRegs::withTwoAvailableRegs(GPRInfo::regT4, GPRInfo::regT2);
    jit.loadValue(CCallHelpers::Address(GPRInfo::regT0, JSBoundFunction::offsetOfBoundThis()), valueRegs);
    jit.storeValue(valueRegs, CCallHelpers::calleeArgumentSlot(0));

    // OK, now we can start copying. This is a simple matter of copying parameters from the caller's
    // frame to the callee's frame. Note that we know that regT3 (the argument count) must be at
    // least 1.
    jit.sub32(CCallHelpers::TrustedImm32(1), GPRInfo::regT3);
    jit.sub32(CCallHelpers::TrustedImm32(1), GPRInfo::regT1);
    CCallHelpers::Jump done = jit.branchTest32(CCallHelpers::Zero, GPRInfo::regT3);
    
    CCallHelpers::Label loop = jit.label();
    jit.sub32(CCallHelpers::TrustedImm32(1), GPRInfo::regT3);
    jit.sub32(CCallHelpers::TrustedImm32(1), GPRInfo::regT1);
    jit.loadValue(CCallHelpers::addressFor(virtualRegisterForArgumentIncludingThis(1)).indexedBy(GPRInfo::regT3, CCallHelpers::TimesEight), valueRegs);
    jit.storeValue(valueRegs, CCallHelpers::calleeArgumentSlot(1).indexedBy(GPRInfo::regT1, CCallHelpers::TimesEight));
    jit.branchTest32(CCallHelpers::NonZero, GPRInfo::regT3).linkTo(loop, &jit);
    
    done.link(&jit);
    auto noArgs2 = jit.branchTest32(CCallHelpers::Zero, GPRInfo::regT1);

    jit.loadPtr(CCallHelpers::Address(GPRInfo::regT0, JSBoundFunction::offsetOfBoundArgs()), GPRInfo::regT3);

    CCallHelpers::Label loopBound = jit.label();
    jit.sub32(CCallHelpers::TrustedImm32(1), GPRInfo::regT1);
    jit.loadValue(CCallHelpers::BaseIndex(GPRInfo::regT3, GPRInfo::regT1, CCallHelpers::TimesEight, JSImmutableButterfly::offsetOfData() + sizeof(WriteBarrier<Unknown>)), valueRegs);
    jit.storeValue(valueRegs, CCallHelpers::calleeArgumentSlot(1).indexedBy(GPRInfo::regT1, CCallHelpers::TimesEight));
    jit.branchTest32(CCallHelpers::NonZero, GPRInfo::regT1).linkTo(loopBound, &jit);

    noArgs2.link(&jit);

    jit.loadPtr(CCallHelpers::Address(GPRInfo::regT0, JSBoundFunction::offsetOfTargetFunction()), GPRInfo::regT2);
    jit.storeCell(GPRInfo::regT2, CCallHelpers::calleeFrameSlot(CallFrameSlot::callee));
    
    jit.loadPtr(CCallHelpers::Address(GPRInfo::regT2, JSFunction::offsetOfExecutableOrRareData()), GPRInfo::regT0);
    auto hasExecutable = jit.branchTestPtr(CCallHelpers::Zero, GPRInfo::regT0, CCallHelpers::TrustedImm32(JSFunction::rareDataTag));
    jit.loadPtr(CCallHelpers::Address(GPRInfo::regT0, FunctionRareData::offsetOfExecutable() - JSFunction::rareDataTag), GPRInfo::regT0);
    hasExecutable.link(&jit);

    jit.loadPtr(
        CCallHelpers::Address(
            GPRInfo::regT0, ExecutableBase::offsetOfJITCodeWithArityCheckFor(CodeForCall)),
        GPRInfo::regT1);
    CCallHelpers::Jump noCode = jit.branchTestPtr(CCallHelpers::Zero, GPRInfo::regT1);

    auto isNative = jit.branchIfNotType(GPRInfo::regT0, FunctionExecutableType);
    jit.loadPtr(
        CCallHelpers::Address(
            GPRInfo::regT0, FunctionExecutable::offsetOfCodeBlockForCall()),
        GPRInfo::regT2);
    jit.storePtr(GPRInfo::regT2, CCallHelpers::calleeFrameCodeBlockBeforeCall());

    isNative.link(&jit);
    
    emitPointerValidation(jit, GPRInfo::regT1, JSEntryPtrTag);
    jit.call(GPRInfo::regT1, JSEntryPtrTag);

    jit.emitFunctionEpilogue();
    jit.ret();
    
    LinkBuffer linkBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::BoundFunctionThunk);
    linkBuffer.link(noCode, CodeLocationLabel<JITThunkPtrTag>(vm.jitStubs->ctiNativeTailCallWithoutSavedTags(vm)));
    return FINALIZE_THUNK(linkBuffer, JITThunkPtrTag, "Specialized thunk for bound function calls with no arguments");
}

MacroAssemblerCodeRef<JITThunkPtrTag> remoteFunctionCallGenerator(VM& vm)
{
    CCallHelpers jit;
    jit.emitFunctionPrologue();

    // Set up our call frame.
    jit.storePtr(CCallHelpers::TrustedImmPtr(nullptr), CCallHelpers::addressFor(CallFrameSlot::codeBlock));
    jit.store32(CCallHelpers::TrustedImm32(0), CCallHelpers::tagFor(CallFrameSlot::argumentCountIncludingThis));

    constexpr unsigned stackMisalignment = sizeof(CallerFrameAndPC) % stackAlignmentBytes();
    constexpr unsigned extraStackNeeded = stackMisalignment ? stackAlignmentBytes() - stackMisalignment : 0;

    // We need to forward all of the arguments that we were passed. We aren't allowed to do a tail
    // call here as far as I can tell. At least not so long as the generic path doesn't do a tail
    // call, since that would be way too weird.

    // The formula for the number of stack bytes needed given some number of parameters (including
    // this) is:
    //
    //     stackAlign((numParams + numFrameLocals + CallFrameHeaderSize) * sizeof(Register) - sizeof(CallerFrameAndPC))
    //
    // Probably we want to write this as:
    //
    //     stackAlign((numParams + numFrameLocals + (CallFrameHeaderSize - CallerFrameAndPCSize)) * sizeof(Register))
    static constexpr int numFrameLocals = 1;
    VirtualRegister loopIndex = virtualRegisterForLocal(0);

    jit.loadCell(CCallHelpers::addressFor(CallFrameSlot::callee), GPRInfo::regT0);
    jit.load32(CCallHelpers::payloadFor(CallFrameSlot::argumentCountIncludingThis), GPRInfo::regT1);

    jit.add32(CCallHelpers::TrustedImm32(CallFrame::headerSizeInRegisters - CallerFrameAndPC::sizeInRegisters + numFrameLocals), GPRInfo::regT1, GPRInfo::regT2);
    jit.lshift32(CCallHelpers::TrustedImm32(3), GPRInfo::regT2);
    jit.add32(CCallHelpers::TrustedImm32(stackAlignmentBytes() - 1), GPRInfo::regT2);
    jit.and32(CCallHelpers::TrustedImm32(-stackAlignmentBytes()), GPRInfo::regT2);

    if (extraStackNeeded)
        jit.add32(CCallHelpers::TrustedImm32(extraStackNeeded), GPRInfo::regT2);

    // At this point regT1 has the actual argument count, and regT2 has the amount of stack we will need.
    // Check to see if we have enough stack space.

    jit.negPtr(GPRInfo::regT2);
    jit.addPtr(CCallHelpers::stackPointerRegister, GPRInfo::regT2);
    CCallHelpers::Jump haveStackSpace = jit.branchPtr(CCallHelpers::BelowOrEqual, CCallHelpers::AbsoluteAddress(vm.addressOfSoftStackLimit()), GPRInfo::regT2);

    // Throw Stack Overflow exception
    jit.copyCalleeSavesToEntryFrameCalleeSavesBuffer(vm.topEntryFrame, GPRInfo::regT3);
    jit.loadPtr(CCallHelpers::Address(GPRInfo::regT0, JSBoundFunction::offsetOfScopeChain()), GPRInfo::regT3);
    jit.setupArguments<decltype(operationThrowStackOverflowErrorFromThunk)>(GPRInfo::regT3);
    jit.prepareCallOperation(vm);
    jit.move(CCallHelpers::TrustedImmPtr(tagCFunction<OperationPtrTag>(operationThrowStackOverflowErrorFromThunk)), GPRInfo::nonArgGPR0);
    emitPointerValidation(jit, GPRInfo::nonArgGPR0, OperationPtrTag);
    jit.call(GPRInfo::nonArgGPR0, OperationPtrTag);
    jit.jumpToExceptionHandler(vm);

    haveStackSpace.link(&jit);
    jit.move(GPRInfo::regT2, CCallHelpers::stackPointerRegister);

    // Set `this` to undefined
    // NOTE: needs concensus in TC39 (https://github.com/tc39/proposal-shadowrealm/issues/328)
    jit.store32(GPRInfo::regT1, CCallHelpers::calleeFramePayloadSlot(CallFrameSlot::argumentCountIncludingThis));
    jit.storeTrustedValue(jsUndefined(), CCallHelpers::calleeArgumentSlot(0));

    JSValueRegs valueRegs = JSValueRegs::withTwoAvailableRegs(GPRInfo::regT4, GPRInfo::regT2);

    // Before processing the arguments loop, check that we have generated JIT code for calling
    // to avoid processing the loop twice in the slow case.
    CCallHelpers::Jump noCode;
    {
        jit.loadPtr(CCallHelpers::Address(GPRInfo::regT0, JSRemoteFunction::offsetOfTargetFunction()), GPRInfo::regT2);
        jit.loadPtr(CCallHelpers::Address(GPRInfo::regT2, JSFunction::offsetOfExecutableOrRareData()), GPRInfo::regT2);
        auto hasExecutable = jit.branchTestPtr(CCallHelpers::Zero, GPRInfo::regT2, CCallHelpers::TrustedImm32(JSFunction::rareDataTag));
        jit.loadPtr(CCallHelpers::Address(GPRInfo::regT2, FunctionRareData::offsetOfExecutable() - JSFunction::rareDataTag), GPRInfo::regT2);
        hasExecutable.link(&jit);

        jit.loadPtr(
            CCallHelpers::Address(
                GPRInfo::regT2, ExecutableBase::offsetOfJITCodeWithArityCheckFor(CodeForCall)),
            GPRInfo::regT2);
        noCode = jit.branchTestPtr(CCallHelpers::Zero, GPRInfo::regT2);
    }

    CCallHelpers::JumpList exceptionChecks;

    // Argument processing loop:
    // For each argument (order should not be observable):
    //     if the value is a Primitive, copy it into the new call frame arguments, otherwise
    //     perform wrapping logic. If the wrapping logic results in a new JSRemoteFunction,
    //     copy it into the new call frame's arguments, otherwise it must have thrown a TypeError.
    CCallHelpers::Jump done = jit.branchSub32(CCallHelpers::Zero, CCallHelpers::TrustedImm32(1), GPRInfo::regT1);
    {
        CCallHelpers::Label loop = jit.label();
        jit.loadValue(CCallHelpers::addressFor(virtualRegisterForArgumentIncludingThis(0)).indexedBy(GPRInfo::regT1, CCallHelpers::TimesEight), valueRegs);

        CCallHelpers::JumpList valueIsPrimitive;
        valueIsPrimitive.append(jit.branchIfNotCell(valueRegs, DoNotHaveTagRegisters));
        valueIsPrimitive.append(jit.branchIfNotObject(valueRegs.payloadGPR()));

        jit.storePtr(GPRInfo::regT1, jit.addressFor(loopIndex));

        jit.setupArguments<decltype(operationGetWrappedValueForTarget)>(GPRInfo::regT0, valueRegs);
        jit.prepareCallOperation(vm);
        jit.move(CCallHelpers::TrustedImmPtr(tagCFunction<OperationPtrTag>(operationGetWrappedValueForTarget)), GPRInfo::nonArgGPR0);
        emitPointerValidation(jit, GPRInfo::nonArgGPR0, OperationPtrTag);
        jit.call(GPRInfo::nonArgGPR0, OperationPtrTag);
        exceptionChecks.append(jit.emitJumpIfException(vm));

        jit.setupResults(valueRegs);
        jit.loadCell(CCallHelpers::addressFor(CallFrameSlot::callee), GPRInfo::regT0);

        jit.loadPtr(jit.addressFor(loopIndex), GPRInfo::regT1);

        valueIsPrimitive.link(&jit);
        jit.storeValue(valueRegs, CCallHelpers::calleeArgumentSlot(0).indexedBy(GPRInfo::regT1, CCallHelpers::TimesEight));
        jit.branchSub32(CCallHelpers::NonZero, CCallHelpers::TrustedImm32(1), GPRInfo::regT1).linkTo(loop, &jit);

        done.link(&jit);
    }

    jit.loadPtr(CCallHelpers::Address(GPRInfo::regT0, JSRemoteFunction::offsetOfTargetFunction()), GPRInfo::regT2);
    jit.storeCell(GPRInfo::regT2, CCallHelpers::calleeFrameSlot(CallFrameSlot::callee));

    jit.loadPtr(CCallHelpers::Address(GPRInfo::regT2, JSFunction::offsetOfExecutableOrRareData()), GPRInfo::regT1);
    auto hasExecutable = jit.branchTestPtr(CCallHelpers::Zero, GPRInfo::regT1, CCallHelpers::TrustedImm32(JSFunction::rareDataTag));
    jit.loadPtr(CCallHelpers::Address(GPRInfo::regT1, FunctionRareData::offsetOfExecutable() - JSFunction::rareDataTag), GPRInfo::regT1);
    hasExecutable.link(&jit);

    jit.loadPtr(
        CCallHelpers::Address(
            GPRInfo::regT1, ExecutableBase::offsetOfJITCodeWithArityCheckFor(CodeForCall)),
        GPRInfo::regT2);
    auto codeExists = jit.branchTestPtr(CCallHelpers::NonZero, GPRInfo::regT2);

    // The calls to operationGetWrappedValueForTarget above may GC, and any GC can potentially jettison the JIT code in the target JSFunction.
    // If we find that the JIT code is null (i.e. has been jettisoned), then we need to re-materialize it for the call below. Note that we know
    // that operationMaterializeRemoteFunctionTargetCode should be able to re-materialize the JIT code (except for any OOME) because we only
    // went down this code path after we found a non-null JIT code (in the noCode check) above i.e. it should be possible to materialize the JIT code.
    // FIXME: Windows x64 is not supported since operationMaterializeRemoteFunctionTargetCode returns SlowPathReturnType.
    jit.setupArguments<decltype(operationMaterializeRemoteFunctionTargetCode)>(GPRInfo::regT0);
    jit.prepareCallOperation(vm);
    jit.move(CCallHelpers::TrustedImmPtr(tagCFunction<OperationPtrTag>(operationMaterializeRemoteFunctionTargetCode)), GPRInfo::nonArgGPR0);
    emitPointerValidation(jit, GPRInfo::nonArgGPR0, OperationPtrTag);
    jit.call(GPRInfo::nonArgGPR0, OperationPtrTag);
    exceptionChecks.append(jit.emitJumpIfException(vm));
    jit.storePtr(GPRInfo::returnValueGPR2, CCallHelpers::calleeFrameCodeBlockBeforeCall());
    jit.move(GPRInfo::returnValueGPR, GPRInfo::regT2);
    auto materialized = jit.jump();

    codeExists.link(&jit);
    auto isNative = jit.branchIfNotType(GPRInfo::regT1, FunctionExecutableType);
    jit.loadPtr(
        CCallHelpers::Address(
            GPRInfo::regT1, FunctionExecutable::offsetOfCodeBlockForCall()),
        GPRInfo::regT3);
    jit.storePtr(GPRInfo::regT3, CCallHelpers::calleeFrameCodeBlockBeforeCall());

    isNative.link(&jit);
    materialized.link(&jit);
    // Based on the check above, we should be good with this. On ARM64, emitPointerValidation will do this.
#if ASSERT_ENABLED && !CPU(ARM64E)
    {
        CCallHelpers::Jump checkNotNull = jit.branchTestPtr(CCallHelpers::NonZero, GPRInfo::regT2);
        jit.abortWithReason(TGInvalidPointer);
        checkNotNull.link(&jit);
    }
#endif

    emitPointerValidation(jit, GPRInfo::regT2, JSEntryPtrTag);
    jit.call(GPRInfo::regT2, JSEntryPtrTag);

    // Wrap return value
    constexpr JSValueRegs resultRegs = JSRInfo::returnValueJSR;

    CCallHelpers::JumpList resultIsPrimitive;
    resultIsPrimitive.append(jit.branchIfNotCell(resultRegs, DoNotHaveTagRegisters));
    resultIsPrimitive.append(jit.branchIfNotObject(resultRegs.payloadGPR()));

    jit.loadCell(CCallHelpers::addressFor(CallFrameSlot::callee), GPRInfo::regT2);
    jit.setupArguments<decltype(operationGetWrappedValueForCaller)>(GPRInfo::regT2, resultRegs);
    jit.prepareCallOperation(vm);
    jit.move(CCallHelpers::TrustedImmPtr(tagCFunction<OperationPtrTag>(operationGetWrappedValueForCaller)), GPRInfo::nonArgGPR0);
    emitPointerValidation(jit, GPRInfo::nonArgGPR0, OperationPtrTag);
    jit.call(GPRInfo::nonArgGPR0, OperationPtrTag);
    exceptionChecks.append(jit.emitJumpIfException(vm));

    resultIsPrimitive.link(&jit);
    jit.emitFunctionEpilogue();
    jit.ret();

    exceptionChecks.link(&jit);
    jit.copyCalleeSavesToEntryFrameCalleeSavesBuffer(vm.topEntryFrame, GPRInfo::argumentGPR0);
    jit.setupArguments<decltype(operationLookupExceptionHandler)>(CCallHelpers::TrustedImmPtr(&vm));
    jit.prepareCallOperation(vm);
    jit.move(CCallHelpers::TrustedImmPtr(tagCFunction<OperationPtrTag>(operationLookupExceptionHandler)), GPRInfo::nonArgGPR0);
    emitPointerValidation(jit, GPRInfo::nonArgGPR0, OperationPtrTag);
    jit.call(GPRInfo::nonArgGPR0, OperationPtrTag);

    jit.jumpToExceptionHandler(vm);

    LinkBuffer linkBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::RemoteFunctionThunk);
    linkBuffer.link(noCode, CodeLocationLabel<JITThunkPtrTag>(vm.jitStubs->ctiNativeTailCallWithoutSavedTags(vm)));
    return FINALIZE_THUNK(linkBuffer, JITThunkPtrTag, "Specialized thunk for remote function calls");
}

} // namespace JSC

#endif // ENABLE(JIT)
