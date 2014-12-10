/*
 * Copyright (C) 2010, 2012, 2013, 2014 Apple Inc. All rights reserved.
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

#include "CodeBlock.h"
#include "DFGSpeculativeJIT.h"
#include "JITOperations.h"
#include "JSArray.h"
#include "JSArrayIterator.h"
#include "JSStack.h"
#include "MaxFrameExtentForSlowPathCall.h"
#include "JSCInlines.h"
#include "SpecializedThunkJIT.h"
#include <wtf/InlineASM.h>
#include <wtf/StringPrintStream.h>
#include <wtf/text/StringImpl.h>

#if ENABLE(JIT)

namespace JSC {

inline void emitPointerValidation(CCallHelpers& jit, GPRReg pointerGPR)
{
    if (ASSERT_DISABLED)
        return;
    CCallHelpers::Jump isNonZero = jit.branchTestPtr(CCallHelpers::NonZero, pointerGPR);
    jit.abortWithReason(TGInvalidPointer);
    isNonZero.link(&jit);
    jit.pushToSave(pointerGPR);
    jit.load8(pointerGPR, pointerGPR);
    jit.popToRestore(pointerGPR);
}

// We will jump here if the JIT code tries to make a call, but the
// linking helper (C++ code) decides to throw an exception instead.
MacroAssemblerCodeRef throwExceptionFromCallSlowPathGenerator(VM* vm)
{
    CCallHelpers jit(vm);
    
    // The call pushed a return address, so we need to pop it back off to re-align the stack,
    // even though we won't use it.
    jit.preserveReturnAddressAfterCall(GPRInfo::nonPreservedNonReturnGPR);

    jit.setupArguments(CCallHelpers::TrustedImmPtr(vm), GPRInfo::callFrameRegister);
    jit.move(CCallHelpers::TrustedImmPtr(bitwise_cast<void*>(lookupExceptionHandler)), GPRInfo::nonArgGPR0);
    emitPointerValidation(jit, GPRInfo::nonArgGPR0);
    jit.call(GPRInfo::nonArgGPR0);
    jit.jumpToExceptionHandler();

    LinkBuffer patchBuffer(*vm, jit, GLOBAL_THUNK_ID);
    return FINALIZE_CODE(patchBuffer, ("Throw exception from call slow path thunk"));
}

static void slowPathFor(
    CCallHelpers& jit, VM* vm, P_JITOperation_ECli slowPathFunction)
{
    jit.emitFunctionPrologue();
    jit.storePtr(GPRInfo::callFrameRegister, &vm->topCallFrame);
    if (maxFrameExtentForSlowPathCall)
        jit.addPtr(CCallHelpers::TrustedImm32(-maxFrameExtentForSlowPathCall), CCallHelpers::stackPointerRegister);
    jit.setupArgumentsWithExecState(GPRInfo::regT2);
    jit.move(CCallHelpers::TrustedImmPtr(bitwise_cast<void*>(slowPathFunction)), GPRInfo::nonArgGPR0);
    emitPointerValidation(jit, GPRInfo::nonArgGPR0);
    jit.call(GPRInfo::nonArgGPR0);
    if (maxFrameExtentForSlowPathCall)
        jit.addPtr(CCallHelpers::TrustedImm32(maxFrameExtentForSlowPathCall), CCallHelpers::stackPointerRegister);
    
    // This slow call will return the address of one of the following:
    // 1) Exception throwing thunk.
    // 2) Host call return value returner thingy.
    // 3) The function to call.
    emitPointerValidation(jit, GPRInfo::returnValueGPR);
    jit.emitFunctionEpilogue();
    jit.jump(GPRInfo::returnValueGPR);
}

static MacroAssemblerCodeRef linkForThunkGenerator(
    VM* vm, CodeSpecializationKind kind, RegisterPreservationMode registers)
{
    // The return address is on the stack or in the link register. We will hence
    // save the return address to the call frame while we make a C++ function call
    // to perform linking and lazy compilation if necessary. We expect the callee
    // to be in regT0/regT1 (payload/tag), the CallFrame to have already
    // been adjusted, and all other registers to be available for use.
    
    CCallHelpers jit(vm);
    
    slowPathFor(jit, vm, operationLinkFor(kind, registers));
    
    LinkBuffer patchBuffer(*vm, jit, GLOBAL_THUNK_ID);
    return FINALIZE_CODE(
        patchBuffer,
        ("Link %s%s slow path thunk", kind == CodeForCall ? "call" : "construct", registers == MustPreserveRegisters ? " that preserves registers" : ""));
}

MacroAssemblerCodeRef linkCallThunkGenerator(VM* vm)
{
    return linkForThunkGenerator(vm, CodeForCall, RegisterPreservationNotRequired);
}

MacroAssemblerCodeRef linkConstructThunkGenerator(VM* vm)
{
    return linkForThunkGenerator(vm, CodeForConstruct, RegisterPreservationNotRequired);
}

MacroAssemblerCodeRef linkCallThatPreservesRegsThunkGenerator(VM* vm)
{
    return linkForThunkGenerator(vm, CodeForCall, MustPreserveRegisters);
}

MacroAssemblerCodeRef linkConstructThatPreservesRegsThunkGenerator(VM* vm)
{
    return linkForThunkGenerator(vm, CodeForConstruct, MustPreserveRegisters);
}

static MacroAssemblerCodeRef linkClosureCallForThunkGenerator(
    VM* vm, RegisterPreservationMode registers)
{
    CCallHelpers jit(vm);
    
    slowPathFor(jit, vm, operationLinkClosureCallFor(registers));
    
    LinkBuffer patchBuffer(*vm, jit, GLOBAL_THUNK_ID);
    return FINALIZE_CODE(patchBuffer, ("Link closure call %s slow path thunk", registers == MustPreserveRegisters ? " that preserves registers" : ""));
}

// For closure optimizations, we only include calls, since if you're using closures for
// object construction then you're going to lose big time anyway.
MacroAssemblerCodeRef linkClosureCallThunkGenerator(VM* vm)
{
    return linkClosureCallForThunkGenerator(vm, RegisterPreservationNotRequired);
}

MacroAssemblerCodeRef linkClosureCallThatPreservesRegsThunkGenerator(VM* vm)
{
    return linkClosureCallForThunkGenerator(vm, MustPreserveRegisters);
}

static MacroAssemblerCodeRef virtualForThunkGenerator(
    VM* vm, CodeSpecializationKind kind, RegisterPreservationMode registers)
{
    // The callee is in regT0 (for JSVALUE32_64, the tag is in regT1).
    // The return address is on the stack, or in the link register. We will hence
    // jump to the callee, or save the return address to the call frame while we
    // make a C++ function call to the appropriate JIT operation.

    CCallHelpers jit(vm);
    
    CCallHelpers::JumpList slowCase;
    
    // This is a slow path execution, and regT2 contains the CallLinkInfo. Count the
    // slow path execution for the profiler.
    jit.add32(
        CCallHelpers::TrustedImm32(1),
        CCallHelpers::Address(GPRInfo::regT2, OBJECT_OFFSETOF(CallLinkInfo, slowPathCount)));

    // FIXME: we should have a story for eliminating these checks. In many cases,
    // the DFG knows that the value is definitely a cell, or definitely a function.
    
#if USE(JSVALUE64)
    jit.move(CCallHelpers::TrustedImm64(TagMask), GPRInfo::regT4);
    
    slowCase.append(
        jit.branchTest64(
            CCallHelpers::NonZero, GPRInfo::regT0, GPRInfo::regT4));
#else
    slowCase.append(
        jit.branch32(
            CCallHelpers::NotEqual, GPRInfo::regT1,
            CCallHelpers::TrustedImm32(JSValue::CellTag)));
#endif
    AssemblyHelpers::emitLoadStructure(jit, GPRInfo::regT0, GPRInfo::regT4, GPRInfo::regT1);
    slowCase.append(
        jit.branchPtr(
            CCallHelpers::NotEqual,
            CCallHelpers::Address(GPRInfo::regT4, Structure::classInfoOffset()),
            CCallHelpers::TrustedImmPtr(JSFunction::info())));
    
    // Now we know we have a JSFunction.
    
    jit.loadPtr(
        CCallHelpers::Address(GPRInfo::regT0, JSFunction::offsetOfExecutable()),
        GPRInfo::regT4);
    jit.loadPtr(
        CCallHelpers::Address(
            GPRInfo::regT4, ExecutableBase::offsetOfJITCodeWithArityCheckFor(kind, registers)),
        GPRInfo::regT4);
    slowCase.append(jit.branchTestPtr(CCallHelpers::Zero, GPRInfo::regT4));
    
    // Now we know that we have a CodeBlock, and we're committed to making a fast
    // call.
    
    jit.loadPtr(
        CCallHelpers::Address(GPRInfo::regT0, JSFunction::offsetOfScopeChain()),
        GPRInfo::regT1);
#if USE(JSVALUE64)
    jit.emitPutToCallFrameHeaderBeforePrologue(GPRInfo::regT1, JSStack::ScopeChain);
#else
    jit.emitPutPayloadToCallFrameHeaderBeforePrologue(GPRInfo::regT1, JSStack::ScopeChain);
    jit.emitPutTagToCallFrameHeaderBeforePrologue(CCallHelpers::TrustedImm32(JSValue::CellTag),
        JSStack::ScopeChain);
#endif
    
    // Make a tail call. This will return back to JIT code.
    emitPointerValidation(jit, GPRInfo::regT4);
    jit.jump(GPRInfo::regT4);

    slowCase.link(&jit);
    
    // Here we don't know anything, so revert to the full slow path.
    
    slowPathFor(jit, vm, operationVirtualFor(kind, registers));
    
    LinkBuffer patchBuffer(*vm, jit, GLOBAL_THUNK_ID);
    return FINALIZE_CODE(
        patchBuffer,
        ("Virtual %s%s slow path thunk", kind == CodeForCall ? "call" : "construct", registers == MustPreserveRegisters ? " that preserves registers" : ""));
}

MacroAssemblerCodeRef virtualCallThunkGenerator(VM* vm)
{
    return virtualForThunkGenerator(vm, CodeForCall, RegisterPreservationNotRequired);
}

MacroAssemblerCodeRef virtualConstructThunkGenerator(VM* vm)
{
    return virtualForThunkGenerator(vm, CodeForConstruct, RegisterPreservationNotRequired);
}

MacroAssemblerCodeRef virtualCallThatPreservesRegsThunkGenerator(VM* vm)
{
    return virtualForThunkGenerator(vm, CodeForCall, MustPreserveRegisters);
}

MacroAssemblerCodeRef virtualConstructThatPreservesRegsThunkGenerator(VM* vm)
{
    return virtualForThunkGenerator(vm, CodeForConstruct, MustPreserveRegisters);
}

enum ThunkEntryType { EnterViaCall, EnterViaJump };

static MacroAssemblerCodeRef nativeForGenerator(VM* vm, CodeSpecializationKind kind, ThunkEntryType entryType = EnterViaCall)
{
    int executableOffsetToFunction = NativeExecutable::offsetOfNativeFunctionFor(kind);
    
    JSInterfaceJIT jit(vm);

    if (entryType == EnterViaCall)
        jit.emitFunctionPrologue();

    jit.emitPutImmediateToCallFrameHeader(0, JSStack::CodeBlock);
    jit.storePtr(JSInterfaceJIT::callFrameRegister, &vm->topCallFrame);

#if CPU(X86)
    // Load caller frame's scope chain into this callframe so that whatever we call can
    // get to its global data.
    jit.emitGetCallerFrameFromCallFrameHeaderPtr(JSInterfaceJIT::regT0);
    jit.emitGetFromCallFrameHeaderPtr(JSStack::ScopeChain, JSInterfaceJIT::regT1, JSInterfaceJIT::regT0);
    jit.emitPutCellToCallFrameHeader(JSInterfaceJIT::regT1, JSStack::ScopeChain);

    // Calling convention:      f(ecx, edx, ...);
    // Host function signature: f(ExecState*);
    jit.move(JSInterfaceJIT::callFrameRegister, X86Registers::ecx);

    jit.subPtr(JSInterfaceJIT::TrustedImm32(8), JSInterfaceJIT::stackPointerRegister); // Align stack after prologue.

    // call the function
    jit.emitGetFromCallFrameHeaderPtr(JSStack::Callee, JSInterfaceJIT::regT1);
    jit.loadPtr(JSInterfaceJIT::Address(JSInterfaceJIT::regT1, JSFunction::offsetOfExecutable()), JSInterfaceJIT::regT1);
    jit.call(JSInterfaceJIT::Address(JSInterfaceJIT::regT1, executableOffsetToFunction));

    jit.addPtr(JSInterfaceJIT::TrustedImm32(8), JSInterfaceJIT::stackPointerRegister);

#elif CPU(X86_64)
    // Load caller frame's scope chain into this callframe so that whatever we call can
    // get to its global data.
    jit.emitGetCallerFrameFromCallFrameHeaderPtr(JSInterfaceJIT::regT0);
    jit.emitGetFromCallFrameHeaderPtr(JSStack::ScopeChain, JSInterfaceJIT::regT1, JSInterfaceJIT::regT0);
    jit.emitPutCellToCallFrameHeader(JSInterfaceJIT::regT1, JSStack::ScopeChain);
#if !OS(WINDOWS)
    // Calling convention:      f(edi, esi, edx, ecx, ...);
    // Host function signature: f(ExecState*);
    jit.move(JSInterfaceJIT::callFrameRegister, X86Registers::edi);

    jit.emitGetFromCallFrameHeaderPtr(JSStack::Callee, X86Registers::esi);
    jit.loadPtr(JSInterfaceJIT::Address(X86Registers::esi, JSFunction::offsetOfExecutable()), X86Registers::r9);
    jit.call(JSInterfaceJIT::Address(X86Registers::r9, executableOffsetToFunction));

#else
    // Calling convention:      f(ecx, edx, r8, r9, ...);
    // Host function signature: f(ExecState*);
    jit.move(JSInterfaceJIT::callFrameRegister, X86Registers::ecx);

    // Leave space for the callee parameter home addresses.
    // At this point the stack is aligned to 16 bytes, but if this changes at some point, we need to emit code to align it.
    jit.subPtr(JSInterfaceJIT::TrustedImm32(4 * sizeof(int64_t)), JSInterfaceJIT::stackPointerRegister);

    jit.emitGetFromCallFrameHeaderPtr(JSStack::Callee, X86Registers::edx);
    jit.loadPtr(JSInterfaceJIT::Address(X86Registers::edx, JSFunction::offsetOfExecutable()), X86Registers::r9);
    jit.call(JSInterfaceJIT::Address(X86Registers::r9, executableOffsetToFunction));

    jit.addPtr(JSInterfaceJIT::TrustedImm32(4 * sizeof(int64_t)), JSInterfaceJIT::stackPointerRegister);
#endif

#elif CPU(ARM64)
    COMPILE_ASSERT(ARM64Registers::x3 != JSInterfaceJIT::regT1, prev_callframe_not_trampled_by_T1);
    COMPILE_ASSERT(ARM64Registers::x3 != JSInterfaceJIT::regT3, prev_callframe_not_trampled_by_T3);
    COMPILE_ASSERT(ARM64Registers::x0 != JSInterfaceJIT::regT3, T3_not_trampled_by_arg_0);
    COMPILE_ASSERT(ARM64Registers::x1 != JSInterfaceJIT::regT3, T3_not_trampled_by_arg_1);
    COMPILE_ASSERT(ARM64Registers::x2 != JSInterfaceJIT::regT3, T3_not_trampled_by_arg_2);

    // Load caller frame's scope chain into this callframe so that whatever we call can
    // get to its global data.
    jit.emitGetCallerFrameFromCallFrameHeaderPtr(ARM64Registers::x3);
    jit.emitGetFromCallFrameHeaderPtr(JSStack::ScopeChain, JSInterfaceJIT::regT1, ARM64Registers::x3);
    jit.emitPutCellToCallFrameHeader(JSInterfaceJIT::regT1, JSStack::ScopeChain);

    // Host function signature: f(ExecState*);
    jit.move(JSInterfaceJIT::callFrameRegister, ARM64Registers::x0);

    jit.emitGetFromCallFrameHeaderPtr(JSStack::Callee, ARM64Registers::x1);
    jit.loadPtr(JSInterfaceJIT::Address(ARM64Registers::x1, JSFunction::offsetOfExecutable()), ARM64Registers::x2);
    jit.call(JSInterfaceJIT::Address(ARM64Registers::x2, executableOffsetToFunction));
#elif CPU(ARM) || CPU(SH4) || CPU(MIPS)
    // Load caller frame's scope chain into this callframe so that whatever we call can get to its global data.
    jit.emitGetCallerFrameFromCallFrameHeaderPtr(JSInterfaceJIT::regT2);
    jit.emitGetFromCallFrameHeaderPtr(JSStack::ScopeChain, JSInterfaceJIT::regT1, JSInterfaceJIT::regT2);
    jit.emitPutCellToCallFrameHeader(JSInterfaceJIT::regT1, JSStack::ScopeChain);

#if CPU(MIPS)
    // Allocate stack space for (unused) 16 bytes (8-byte aligned) for 4 arguments.
    jit.subPtr(JSInterfaceJIT::TrustedImm32(16), JSInterfaceJIT::stackPointerRegister);
#endif

    // Calling convention is f(argumentGPR0, argumentGPR1, ...).
    // Host function signature is f(ExecState*).
    jit.move(JSInterfaceJIT::callFrameRegister, JSInterfaceJIT::argumentGPR0);

    jit.emitGetFromCallFrameHeaderPtr(JSStack::Callee, JSInterfaceJIT::argumentGPR1);
    jit.loadPtr(JSInterfaceJIT::Address(JSInterfaceJIT::argumentGPR1, JSFunction::offsetOfExecutable()), JSInterfaceJIT::regT2);
    jit.call(JSInterfaceJIT::Address(JSInterfaceJIT::regT2, executableOffsetToFunction));

#if CPU(MIPS)
    // Restore stack space
    jit.addPtr(JSInterfaceJIT::TrustedImm32(16), JSInterfaceJIT::stackPointerRegister);
#endif
#else
#error "JIT not supported on this platform."
    UNUSED_PARAM(executableOffsetToFunction);
    abortWithReason(TGNotSupported);
#endif

    // Check for an exception
#if USE(JSVALUE64)
    jit.load64(vm->addressOfException(), JSInterfaceJIT::regT2);
    JSInterfaceJIT::Jump exceptionHandler = jit.branchTest64(JSInterfaceJIT::NonZero, JSInterfaceJIT::regT2);
#else
    JSInterfaceJIT::Jump exceptionHandler = jit.branch32(
        JSInterfaceJIT::NotEqual,
        JSInterfaceJIT::AbsoluteAddress(reinterpret_cast<char*>(vm->addressOfException()) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.tag)),
        JSInterfaceJIT::TrustedImm32(JSValue::EmptyValueTag));
#endif

    jit.emitFunctionEpilogue();
    // Return.
    jit.ret();

    // Handle an exception
    exceptionHandler.link(&jit);

    jit.storePtr(JSInterfaceJIT::callFrameRegister, &vm->topCallFrame);

#if CPU(X86) && USE(JSVALUE32_64)
    jit.addPtr(JSInterfaceJIT::TrustedImm32(-12), JSInterfaceJIT::stackPointerRegister);
    jit.loadPtr(JSInterfaceJIT::Address(JSInterfaceJIT::callFrameRegister), JSInterfaceJIT::regT0);
    jit.push(JSInterfaceJIT::regT0);
#else
#if OS(WINDOWS)
    // Allocate space on stack for the 4 parameter registers.
    jit.subPtr(JSInterfaceJIT::TrustedImm32(4 * sizeof(int64_t)), JSInterfaceJIT::stackPointerRegister);
#endif
    jit.loadPtr(JSInterfaceJIT::Address(JSInterfaceJIT::callFrameRegister), JSInterfaceJIT::argumentGPR0);
#endif
    jit.move(JSInterfaceJIT::TrustedImmPtr(FunctionPtr(operationVMHandleException).value()), JSInterfaceJIT::regT3);
    jit.call(JSInterfaceJIT::regT3);
#if CPU(X86) && USE(JSVALUE32_64)
    jit.addPtr(JSInterfaceJIT::TrustedImm32(16), JSInterfaceJIT::stackPointerRegister);
#elif OS(WINDOWS)
    jit.addPtr(JSInterfaceJIT::TrustedImm32(4 * sizeof(int64_t)), JSInterfaceJIT::stackPointerRegister);
#endif

    jit.jumpToExceptionHandler();

    LinkBuffer patchBuffer(*vm, jit, GLOBAL_THUNK_ID);
    return FINALIZE_CODE(patchBuffer, ("native %s%s trampoline", entryType == EnterViaJump ? "Tail " : "", toCString(kind).data()));
}

MacroAssemblerCodeRef nativeCallGenerator(VM* vm)
{
    return nativeForGenerator(vm, CodeForCall);
}

MacroAssemblerCodeRef nativeTailCallGenerator(VM* vm)
{
    return nativeForGenerator(vm, CodeForCall, EnterViaJump);
}

MacroAssemblerCodeRef nativeConstructGenerator(VM* vm)
{
    return nativeForGenerator(vm, CodeForConstruct);
}

MacroAssemblerCodeRef arityFixup(VM* vm)
{
    JSInterfaceJIT jit(vm);

    // We enter with fixup count, in aligned stack units, in regT0 and the return thunk in
    // regT5 on 32-bit and regT7 on 64-bit.
#if USE(JSVALUE64)
#  if CPU(X86_64)
    jit.pop(JSInterfaceJIT::regT4);
#  endif
    jit.lshift32(JSInterfaceJIT::TrustedImm32(logStackAlignmentRegisters()), JSInterfaceJIT::regT0);
    jit.neg64(JSInterfaceJIT::regT0);
    jit.move(JSInterfaceJIT::callFrameRegister, JSInterfaceJIT::regT6);
    jit.load32(JSInterfaceJIT::Address(JSInterfaceJIT::callFrameRegister, JSStack::ArgumentCount * sizeof(Register)), JSInterfaceJIT::regT2);
    jit.add32(JSInterfaceJIT::TrustedImm32(JSStack::CallFrameHeaderSize), JSInterfaceJIT::regT2);

    // Move current frame down regT0 number of slots
    JSInterfaceJIT::Label copyLoop(jit.label());
    jit.load64(JSInterfaceJIT::regT6, JSInterfaceJIT::regT1);
    jit.store64(JSInterfaceJIT::regT1, MacroAssembler::BaseIndex(JSInterfaceJIT::regT6, JSInterfaceJIT::regT0, JSInterfaceJIT::TimesEight));
    jit.addPtr(JSInterfaceJIT::TrustedImm32(8), JSInterfaceJIT::regT6);
    jit.branchSub32(MacroAssembler::NonZero, JSInterfaceJIT::TrustedImm32(1), JSInterfaceJIT::regT2).linkTo(copyLoop, &jit);

    // Fill in regT0 - 1 missing arg slots with undefined
    jit.move(JSInterfaceJIT::regT0, JSInterfaceJIT::regT2);
    jit.move(JSInterfaceJIT::TrustedImm64(ValueUndefined), JSInterfaceJIT::regT1);
    jit.add32(JSInterfaceJIT::TrustedImm32(1), JSInterfaceJIT::regT2);
    JSInterfaceJIT::Label fillUndefinedLoop(jit.label());
    jit.store64(JSInterfaceJIT::regT1, MacroAssembler::BaseIndex(JSInterfaceJIT::regT6, JSInterfaceJIT::regT0, JSInterfaceJIT::TimesEight));
    jit.addPtr(JSInterfaceJIT::TrustedImm32(8), JSInterfaceJIT::regT6);
    jit.branchAdd32(MacroAssembler::NonZero, JSInterfaceJIT::TrustedImm32(1), JSInterfaceJIT::regT2).linkTo(fillUndefinedLoop, &jit);
    
    // Adjust call frame register and stack pointer to account for missing args
    jit.move(JSInterfaceJIT::regT0, JSInterfaceJIT::regT1);
    jit.lshift64(JSInterfaceJIT::TrustedImm32(3), JSInterfaceJIT::regT1);
    jit.addPtr(JSInterfaceJIT::regT1, JSInterfaceJIT::callFrameRegister);
    jit.addPtr(JSInterfaceJIT::regT1, JSInterfaceJIT::stackPointerRegister);

    // Save the original return PC.
    jit.loadPtr(JSInterfaceJIT::Address(JSInterfaceJIT::callFrameRegister, CallFrame::returnPCOffset()), GPRInfo::regT1);
    jit.storePtr(GPRInfo::regT1, MacroAssembler::BaseIndex(JSInterfaceJIT::regT6, JSInterfaceJIT::regT0, JSInterfaceJIT::TimesEight));
    
    // Install the new return PC.
    jit.storePtr(GPRInfo::regT7, JSInterfaceJIT::Address(JSInterfaceJIT::callFrameRegister, CallFrame::returnPCOffset()));

#  if CPU(X86_64)
    jit.push(JSInterfaceJIT::regT4);
#  endif
    jit.ret();
#else
#  if CPU(X86)
    jit.pop(JSInterfaceJIT::regT4);
#  endif
    jit.lshift32(JSInterfaceJIT::TrustedImm32(logStackAlignmentRegisters()), JSInterfaceJIT::regT0);
    jit.neg32(JSInterfaceJIT::regT0);
    jit.move(JSInterfaceJIT::callFrameRegister, JSInterfaceJIT::regT3);
    jit.load32(JSInterfaceJIT::Address(JSInterfaceJIT::callFrameRegister, JSStack::ArgumentCount * sizeof(Register)), JSInterfaceJIT::regT2);
    jit.add32(JSInterfaceJIT::TrustedImm32(JSStack::CallFrameHeaderSize), JSInterfaceJIT::regT2);

    // Move current frame down regT0 number of slots
    JSInterfaceJIT::Label copyLoop(jit.label());
    jit.load32(JSInterfaceJIT::regT3, JSInterfaceJIT::regT1);
    jit.store32(JSInterfaceJIT::regT1, MacroAssembler::BaseIndex(JSInterfaceJIT::regT3, JSInterfaceJIT::regT0, JSInterfaceJIT::TimesEight));
    jit.load32(MacroAssembler::Address(JSInterfaceJIT::regT3, 4), JSInterfaceJIT::regT1);
    jit.store32(JSInterfaceJIT::regT1, MacroAssembler::BaseIndex(JSInterfaceJIT::regT3, JSInterfaceJIT::regT0, JSInterfaceJIT::TimesEight, 4));
    jit.addPtr(JSInterfaceJIT::TrustedImm32(8), JSInterfaceJIT::regT3);
    jit.branchSub32(MacroAssembler::NonZero, JSInterfaceJIT::TrustedImm32(1), JSInterfaceJIT::regT2).linkTo(copyLoop, &jit);

    // Fill in regT0 - 1 missing arg slots with undefined
    jit.move(JSInterfaceJIT::regT0, JSInterfaceJIT::regT2);
    jit.add32(JSInterfaceJIT::TrustedImm32(1), JSInterfaceJIT::regT2);
    JSInterfaceJIT::Label fillUndefinedLoop(jit.label());
    jit.move(JSInterfaceJIT::TrustedImm32(0), JSInterfaceJIT::regT1);
    jit.store32(JSInterfaceJIT::regT1, MacroAssembler::BaseIndex(JSInterfaceJIT::regT3, JSInterfaceJIT::regT0, JSInterfaceJIT::TimesEight));
    jit.move(JSInterfaceJIT::TrustedImm32(JSValue::UndefinedTag), JSInterfaceJIT::regT1);
    jit.store32(JSInterfaceJIT::regT1, MacroAssembler::BaseIndex(JSInterfaceJIT::regT3, JSInterfaceJIT::regT0, JSInterfaceJIT::TimesEight, 4));

    jit.addPtr(JSInterfaceJIT::TrustedImm32(8), JSInterfaceJIT::regT3);
    jit.branchAdd32(MacroAssembler::NonZero, JSInterfaceJIT::TrustedImm32(1), JSInterfaceJIT::regT2).linkTo(fillUndefinedLoop, &jit);

    // Adjust call frame register and stack pointer to account for missing args
    jit.move(JSInterfaceJIT::regT0, JSInterfaceJIT::regT1);
    jit.lshift32(JSInterfaceJIT::TrustedImm32(3), JSInterfaceJIT::regT1);
    jit.addPtr(JSInterfaceJIT::regT1, JSInterfaceJIT::callFrameRegister);
    jit.addPtr(JSInterfaceJIT::regT1, JSInterfaceJIT::stackPointerRegister);

    // Save the original return PC.
    jit.loadPtr(JSInterfaceJIT::Address(JSInterfaceJIT::callFrameRegister, CallFrame::returnPCOffset()), GPRInfo::regT1);
    jit.storePtr(GPRInfo::regT1, MacroAssembler::BaseIndex(JSInterfaceJIT::regT3, JSInterfaceJIT::regT0, JSInterfaceJIT::TimesEight));
    
    // Install the new return PC.
    jit.storePtr(GPRInfo::regT5, JSInterfaceJIT::Address(JSInterfaceJIT::callFrameRegister, CallFrame::returnPCOffset()));
    
#  if CPU(X86)
    jit.push(JSInterfaceJIT::regT4);
#  endif
    jit.ret();
#endif

    LinkBuffer patchBuffer(*vm, jit, GLOBAL_THUNK_ID);
    return FINALIZE_CODE(patchBuffer, ("fixup arity"));
}

static void stringCharLoad(SpecializedThunkJIT& jit, VM* vm)
{
    // load string
    jit.loadJSStringArgument(*vm, SpecializedThunkJIT::ThisArgument, SpecializedThunkJIT::regT0);

    // Load string length to regT2, and start the process of loading the data pointer into regT0
    jit.load32(MacroAssembler::Address(SpecializedThunkJIT::regT0, ThunkHelpers::jsStringLengthOffset()), SpecializedThunkJIT::regT2);
    jit.loadPtr(MacroAssembler::Address(SpecializedThunkJIT::regT0, ThunkHelpers::jsStringValueOffset()), SpecializedThunkJIT::regT0);
    jit.appendFailure(jit.branchTest32(MacroAssembler::Zero, SpecializedThunkJIT::regT0));

    // load index
    jit.loadInt32Argument(0, SpecializedThunkJIT::regT1); // regT1 contains the index

    // Do an unsigned compare to simultaneously filter negative indices as well as indices that are too large
    jit.appendFailure(jit.branch32(MacroAssembler::AboveOrEqual, SpecializedThunkJIT::regT1, SpecializedThunkJIT::regT2));

    // Load the character
    SpecializedThunkJIT::JumpList is16Bit;
    SpecializedThunkJIT::JumpList cont8Bit;
    // Load the string flags
    jit.loadPtr(MacroAssembler::Address(SpecializedThunkJIT::regT0, StringImpl::flagsOffset()), SpecializedThunkJIT::regT2);
    jit.loadPtr(MacroAssembler::Address(SpecializedThunkJIT::regT0, StringImpl::dataOffset()), SpecializedThunkJIT::regT0);
    is16Bit.append(jit.branchTest32(MacroAssembler::Zero, SpecializedThunkJIT::regT2, MacroAssembler::TrustedImm32(StringImpl::flagIs8Bit())));
    jit.load8(MacroAssembler::BaseIndex(SpecializedThunkJIT::regT0, SpecializedThunkJIT::regT1, MacroAssembler::TimesOne, 0), SpecializedThunkJIT::regT0);
    cont8Bit.append(jit.jump());
    is16Bit.link(&jit);
    jit.load16(MacroAssembler::BaseIndex(SpecializedThunkJIT::regT0, SpecializedThunkJIT::regT1, MacroAssembler::TimesTwo, 0), SpecializedThunkJIT::regT0);
    cont8Bit.link(&jit);
}

static void charToString(SpecializedThunkJIT& jit, VM* vm, MacroAssembler::RegisterID src, MacroAssembler::RegisterID dst, MacroAssembler::RegisterID scratch)
{
    jit.appendFailure(jit.branch32(MacroAssembler::AboveOrEqual, src, MacroAssembler::TrustedImm32(0x100)));
    jit.move(MacroAssembler::TrustedImmPtr(vm->smallStrings.singleCharacterStrings()), scratch);
    jit.loadPtr(MacroAssembler::BaseIndex(scratch, src, MacroAssembler::ScalePtr, 0), dst);
    jit.appendFailure(jit.branchTestPtr(MacroAssembler::Zero, dst));
}

MacroAssemblerCodeRef charCodeAtThunkGenerator(VM* vm)
{
    SpecializedThunkJIT jit(vm, 1);
    stringCharLoad(jit, vm);
    jit.returnInt32(SpecializedThunkJIT::regT0);
    return jit.finalize(vm->jitStubs->ctiNativeTailCall(vm), "charCodeAt");
}

MacroAssemblerCodeRef charAtThunkGenerator(VM* vm)
{
    SpecializedThunkJIT jit(vm, 1);
    stringCharLoad(jit, vm);
    charToString(jit, vm, SpecializedThunkJIT::regT0, SpecializedThunkJIT::regT0, SpecializedThunkJIT::regT1);
    jit.returnJSCell(SpecializedThunkJIT::regT0);
    return jit.finalize(vm->jitStubs->ctiNativeTailCall(vm), "charAt");
}

MacroAssemblerCodeRef fromCharCodeThunkGenerator(VM* vm)
{
    SpecializedThunkJIT jit(vm, 1);
    // load char code
    jit.loadInt32Argument(0, SpecializedThunkJIT::regT0);
    charToString(jit, vm, SpecializedThunkJIT::regT0, SpecializedThunkJIT::regT0, SpecializedThunkJIT::regT1);
    jit.returnJSCell(SpecializedThunkJIT::regT0);
    return jit.finalize(vm->jitStubs->ctiNativeTailCall(vm), "fromCharCode");
}

MacroAssemblerCodeRef sqrtThunkGenerator(VM* vm)
{
    SpecializedThunkJIT jit(vm, 1);
    if (!jit.supportsFloatingPointSqrt())
        return MacroAssemblerCodeRef::createSelfManagedCodeRef(vm->jitStubs->ctiNativeCall(vm));

    jit.loadDoubleArgument(0, SpecializedThunkJIT::fpRegT0, SpecializedThunkJIT::regT0);
    jit.sqrtDouble(SpecializedThunkJIT::fpRegT0, SpecializedThunkJIT::fpRegT0);
    jit.returnDouble(SpecializedThunkJIT::fpRegT0);
    return jit.finalize(vm->jitStubs->ctiNativeTailCall(vm), "sqrt");
}


#define UnaryDoubleOpWrapper(function) function##Wrapper
enum MathThunkCallingConvention { };
typedef MathThunkCallingConvention(*MathThunk)(MathThunkCallingConvention);
extern "C" {

double jsRound(double) REFERENCED_FROM_ASM;
double jsRound(double d)
{
    double integer = ceil(d);
    return integer - (integer - d > 0.5);
}

}

#if CPU(X86_64) && COMPILER(GCC) && (OS(DARWIN) || OS(LINUX))

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
    );\
    extern "C" { \
        MathThunkCallingConvention function##Thunk(MathThunkCallingConvention); \
    } \
    static MathThunk UnaryDoubleOpWrapper(function) = &function##Thunk;

#elif CPU(X86) && COMPILER(GCC) && (OS(DARWIN) || OS(LINUX))
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
    );\
    extern "C" { \
        MathThunkCallingConvention function##Thunk(MathThunkCallingConvention); \
    } \
    static MathThunk UnaryDoubleOpWrapper(function) = &function##Thunk;

#elif CPU(ARM_THUMB2) && COMPILER(GCC) && PLATFORM(IOS)

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
    ); \
    extern "C" { \
        MathThunkCallingConvention function##Thunk(MathThunkCallingConvention); \
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
    ); \
    extern "C" { \
        MathThunkCallingConvention function##Thunk(MathThunkCallingConvention); \
    } \
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

static const double oneConstant = 1.0;
static const double negativeHalfConstant = -0.5;
static const double zeroConstant = 0.0;
static const double halfConstant = 0.5;
    
MacroAssemblerCodeRef floorThunkGenerator(VM* vm)
{
    SpecializedThunkJIT jit(vm, 1);
    MacroAssembler::Jump nonIntJump;
    if (!UnaryDoubleOpWrapper(floor) || !jit.supportsFloatingPoint())
        return MacroAssemblerCodeRef::createSelfManagedCodeRef(vm->jitStubs->ctiNativeCall(vm));
    jit.loadInt32Argument(0, SpecializedThunkJIT::regT0, nonIntJump);
    jit.returnInt32(SpecializedThunkJIT::regT0);
    nonIntJump.link(&jit);
    jit.loadDoubleArgument(0, SpecializedThunkJIT::fpRegT0, SpecializedThunkJIT::regT0);
#if CPU(ARM64)
    SpecializedThunkJIT::JumpList doubleResult;
    jit.floorDouble(SpecializedThunkJIT::fpRegT0, SpecializedThunkJIT::fpRegT0);
    jit.branchConvertDoubleToInt32(SpecializedThunkJIT::fpRegT0, SpecializedThunkJIT::regT0, doubleResult, SpecializedThunkJIT::fpRegT1);
    jit.returnInt32(SpecializedThunkJIT::regT0);
    doubleResult.link(&jit);
    jit.returnDouble(SpecializedThunkJIT::fpRegT0);
#else
    SpecializedThunkJIT::Jump intResult;
    SpecializedThunkJIT::JumpList doubleResult;
    if (jit.supportsFloatingPointTruncate()) {
        jit.loadDouble(MacroAssembler::TrustedImmPtr(&zeroConstant), SpecializedThunkJIT::fpRegT1);
        doubleResult.append(jit.branchDouble(MacroAssembler::DoubleEqual, SpecializedThunkJIT::fpRegT0, SpecializedThunkJIT::fpRegT1));
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
#endif // CPU(ARM64)
    return jit.finalize(vm->jitStubs->ctiNativeTailCall(vm), "floor");
}

MacroAssemblerCodeRef ceilThunkGenerator(VM* vm)
{
    SpecializedThunkJIT jit(vm, 1);
    if (!UnaryDoubleOpWrapper(ceil) || !jit.supportsFloatingPoint())
        return MacroAssemblerCodeRef::createSelfManagedCodeRef(vm->jitStubs->ctiNativeCall(vm));
    MacroAssembler::Jump nonIntJump;
    jit.loadInt32Argument(0, SpecializedThunkJIT::regT0, nonIntJump);
    jit.returnInt32(SpecializedThunkJIT::regT0);
    nonIntJump.link(&jit);
    jit.loadDoubleArgument(0, SpecializedThunkJIT::fpRegT0, SpecializedThunkJIT::regT0);
#if CPU(ARM64)
    jit.ceilDouble(SpecializedThunkJIT::fpRegT0, SpecializedThunkJIT::fpRegT0);
#else
    jit.callDoubleToDoublePreservingReturn(UnaryDoubleOpWrapper(ceil));
#endif // CPU(ARM64)
    SpecializedThunkJIT::JumpList doubleResult;
    jit.branchConvertDoubleToInt32(SpecializedThunkJIT::fpRegT0, SpecializedThunkJIT::regT0, doubleResult, SpecializedThunkJIT::fpRegT1);
    jit.returnInt32(SpecializedThunkJIT::regT0);
    doubleResult.link(&jit);
    jit.returnDouble(SpecializedThunkJIT::fpRegT0);
    return jit.finalize(vm->jitStubs->ctiNativeTailCall(vm), "ceil");
}

MacroAssemblerCodeRef roundThunkGenerator(VM* vm)
{
    SpecializedThunkJIT jit(vm, 1);
    if (!UnaryDoubleOpWrapper(jsRound) || !jit.supportsFloatingPoint())
        return MacroAssemblerCodeRef::createSelfManagedCodeRef(vm->jitStubs->ctiNativeCall(vm));
    MacroAssembler::Jump nonIntJump;
    jit.loadInt32Argument(0, SpecializedThunkJIT::regT0, nonIntJump);
    jit.returnInt32(SpecializedThunkJIT::regT0);
    nonIntJump.link(&jit);
    jit.loadDoubleArgument(0, SpecializedThunkJIT::fpRegT0, SpecializedThunkJIT::regT0);
    SpecializedThunkJIT::Jump intResult;
    SpecializedThunkJIT::JumpList doubleResult;
    if (jit.supportsFloatingPointTruncate()) {
        jit.loadDouble(MacroAssembler::TrustedImmPtr(&zeroConstant), SpecializedThunkJIT::fpRegT1);
        doubleResult.append(jit.branchDouble(MacroAssembler::DoubleEqual, SpecializedThunkJIT::fpRegT0, SpecializedThunkJIT::fpRegT1));
        SpecializedThunkJIT::JumpList slowPath;
        // Handle the negative doubles in the slow path for now.
        slowPath.append(jit.branchDouble(MacroAssembler::DoubleLessThanOrUnordered, SpecializedThunkJIT::fpRegT0, SpecializedThunkJIT::fpRegT1));
        jit.loadDouble(MacroAssembler::TrustedImmPtr(&halfConstant), SpecializedThunkJIT::fpRegT1);
        jit.addDouble(SpecializedThunkJIT::fpRegT0, SpecializedThunkJIT::fpRegT1);
        slowPath.append(jit.branchTruncateDoubleToInt32(SpecializedThunkJIT::fpRegT1, SpecializedThunkJIT::regT0));
        intResult = jit.jump();
        slowPath.link(&jit);
    }
    jit.callDoubleToDoublePreservingReturn(UnaryDoubleOpWrapper(jsRound));
    jit.branchConvertDoubleToInt32(SpecializedThunkJIT::fpRegT0, SpecializedThunkJIT::regT0, doubleResult, SpecializedThunkJIT::fpRegT1);
    if (jit.supportsFloatingPointTruncate())
        intResult.link(&jit);
    jit.returnInt32(SpecializedThunkJIT::regT0);
    doubleResult.link(&jit);
    jit.returnDouble(SpecializedThunkJIT::fpRegT0);
    return jit.finalize(vm->jitStubs->ctiNativeTailCall(vm), "round");
}

MacroAssemblerCodeRef expThunkGenerator(VM* vm)
{
    if (!UnaryDoubleOpWrapper(exp))
        return MacroAssemblerCodeRef::createSelfManagedCodeRef(vm->jitStubs->ctiNativeCall(vm));
    SpecializedThunkJIT jit(vm, 1);
    if (!jit.supportsFloatingPoint())
        return MacroAssemblerCodeRef::createSelfManagedCodeRef(vm->jitStubs->ctiNativeCall(vm));
    jit.loadDoubleArgument(0, SpecializedThunkJIT::fpRegT0, SpecializedThunkJIT::regT0);
    jit.callDoubleToDoublePreservingReturn(UnaryDoubleOpWrapper(exp));
    jit.returnDouble(SpecializedThunkJIT::fpRegT0);
    return jit.finalize(vm->jitStubs->ctiNativeTailCall(vm), "exp");
}

MacroAssemblerCodeRef logThunkGenerator(VM* vm)
{
    if (!UnaryDoubleOpWrapper(log))
        return MacroAssemblerCodeRef::createSelfManagedCodeRef(vm->jitStubs->ctiNativeCall(vm));
    SpecializedThunkJIT jit(vm, 1);
    if (!jit.supportsFloatingPoint())
        return MacroAssemblerCodeRef::createSelfManagedCodeRef(vm->jitStubs->ctiNativeCall(vm));
    jit.loadDoubleArgument(0, SpecializedThunkJIT::fpRegT0, SpecializedThunkJIT::regT0);
    jit.callDoubleToDoublePreservingReturn(UnaryDoubleOpWrapper(log));
    jit.returnDouble(SpecializedThunkJIT::fpRegT0);
    return jit.finalize(vm->jitStubs->ctiNativeTailCall(vm), "log");
}

MacroAssemblerCodeRef absThunkGenerator(VM* vm)
{
    SpecializedThunkJIT jit(vm, 1);
    if (!jit.supportsFloatingPointAbs())
        return MacroAssemblerCodeRef::createSelfManagedCodeRef(vm->jitStubs->ctiNativeCall(vm));
    MacroAssembler::Jump nonIntJump;
    jit.loadInt32Argument(0, SpecializedThunkJIT::regT0, nonIntJump);
    jit.rshift32(SpecializedThunkJIT::regT0, MacroAssembler::TrustedImm32(31), SpecializedThunkJIT::regT1);
    jit.add32(SpecializedThunkJIT::regT1, SpecializedThunkJIT::regT0);
    jit.xor32(SpecializedThunkJIT::regT1, SpecializedThunkJIT::regT0);
    jit.appendFailure(jit.branch32(MacroAssembler::Equal, SpecializedThunkJIT::regT0, MacroAssembler::TrustedImm32(1 << 31)));
    jit.returnInt32(SpecializedThunkJIT::regT0);
    nonIntJump.link(&jit);
    // Shame about the double int conversion here.
    jit.loadDoubleArgument(0, SpecializedThunkJIT::fpRegT0, SpecializedThunkJIT::regT0);
    jit.absDouble(SpecializedThunkJIT::fpRegT0, SpecializedThunkJIT::fpRegT1);
    jit.returnDouble(SpecializedThunkJIT::fpRegT1);
    return jit.finalize(vm->jitStubs->ctiNativeTailCall(vm), "abs");
}

MacroAssemblerCodeRef powThunkGenerator(VM* vm)
{
    SpecializedThunkJIT jit(vm, 2);
    if (!jit.supportsFloatingPoint())
        return MacroAssemblerCodeRef::createSelfManagedCodeRef(vm->jitStubs->ctiNativeCall(vm));

    jit.loadDouble(MacroAssembler::TrustedImmPtr(&oneConstant), SpecializedThunkJIT::fpRegT1);
    jit.loadDoubleArgument(0, SpecializedThunkJIT::fpRegT0, SpecializedThunkJIT::regT0);
    MacroAssembler::Jump nonIntExponent;
    jit.loadInt32Argument(1, SpecializedThunkJIT::regT0, nonIntExponent);
    jit.appendFailure(jit.branch32(MacroAssembler::LessThan, SpecializedThunkJIT::regT0, MacroAssembler::TrustedImm32(0)));
    
    MacroAssembler::Jump exponentIsZero = jit.branchTest32(MacroAssembler::Zero, SpecializedThunkJIT::regT0);
    MacroAssembler::Label startLoop(jit.label());

    MacroAssembler::Jump exponentIsEven = jit.branchTest32(MacroAssembler::Zero, SpecializedThunkJIT::regT0, MacroAssembler::TrustedImm32(1));
    jit.mulDouble(SpecializedThunkJIT::fpRegT0, SpecializedThunkJIT::fpRegT1);
    exponentIsEven.link(&jit);
    jit.mulDouble(SpecializedThunkJIT::fpRegT0, SpecializedThunkJIT::fpRegT0);
    jit.rshift32(MacroAssembler::TrustedImm32(1), SpecializedThunkJIT::regT0);
    jit.branchTest32(MacroAssembler::NonZero, SpecializedThunkJIT::regT0).linkTo(startLoop, &jit);

    exponentIsZero.link(&jit);

    {
        SpecializedThunkJIT::JumpList doubleResult;
        jit.branchConvertDoubleToInt32(SpecializedThunkJIT::fpRegT1, SpecializedThunkJIT::regT0, doubleResult, SpecializedThunkJIT::fpRegT0);
        jit.returnInt32(SpecializedThunkJIT::regT0);
        doubleResult.link(&jit);
        jit.returnDouble(SpecializedThunkJIT::fpRegT1);
    }

    if (jit.supportsFloatingPointSqrt()) {
        nonIntExponent.link(&jit);
        jit.loadDouble(MacroAssembler::TrustedImmPtr(&negativeHalfConstant), SpecializedThunkJIT::fpRegT3);
        jit.loadDoubleArgument(1, SpecializedThunkJIT::fpRegT2, SpecializedThunkJIT::regT0);
        jit.appendFailure(jit.branchDouble(MacroAssembler::DoubleLessThanOrEqual, SpecializedThunkJIT::fpRegT0, SpecializedThunkJIT::fpRegT1));
        jit.appendFailure(jit.branchDouble(MacroAssembler::DoubleNotEqualOrUnordered, SpecializedThunkJIT::fpRegT2, SpecializedThunkJIT::fpRegT3));
        jit.sqrtDouble(SpecializedThunkJIT::fpRegT0, SpecializedThunkJIT::fpRegT0);
        jit.divDouble(SpecializedThunkJIT::fpRegT0, SpecializedThunkJIT::fpRegT1);

        SpecializedThunkJIT::JumpList doubleResult;
        jit.branchConvertDoubleToInt32(SpecializedThunkJIT::fpRegT1, SpecializedThunkJIT::regT0, doubleResult, SpecializedThunkJIT::fpRegT0);
        jit.returnInt32(SpecializedThunkJIT::regT0);
        doubleResult.link(&jit);
        jit.returnDouble(SpecializedThunkJIT::fpRegT1);
    } else
        jit.appendFailure(nonIntExponent);

    return jit.finalize(vm->jitStubs->ctiNativeTailCall(vm), "pow");
}

MacroAssemblerCodeRef imulThunkGenerator(VM* vm)
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

    return jit.finalize(vm->jitStubs->ctiNativeTailCall(vm), "imul");
}

static MacroAssemblerCodeRef arrayIteratorNextThunkGenerator(VM* vm, ArrayIterationKind kind)
{
    typedef SpecializedThunkJIT::TrustedImm32 TrustedImm32;
    typedef SpecializedThunkJIT::TrustedImmPtr TrustedImmPtr;
    typedef SpecializedThunkJIT::Address Address;
    typedef SpecializedThunkJIT::BaseIndex BaseIndex;
    typedef SpecializedThunkJIT::Jump Jump;
    
    SpecializedThunkJIT jit(vm);
    // Make sure we're being called on an array iterator, and load m_iteratedObject, and m_nextIndex into regT0 and regT1 respectively
    jit.loadArgumentWithSpecificClass(JSArrayIterator::info(), SpecializedThunkJIT::ThisArgument, SpecializedThunkJIT::regT4, SpecializedThunkJIT::regT1);

    // Early exit if we don't have a thunk for this form of iteration
    jit.appendFailure(jit.branch32(SpecializedThunkJIT::AboveOrEqual, Address(SpecializedThunkJIT::regT4, JSArrayIterator::offsetOfIterationKind()), TrustedImm32(ArrayIterateKeyValue)));
    
    jit.loadPtr(Address(SpecializedThunkJIT::regT4, JSArrayIterator::offsetOfIteratedObject()), SpecializedThunkJIT::regT0);
    
    jit.load32(Address(SpecializedThunkJIT::regT4, JSArrayIterator::offsetOfNextIndex()), SpecializedThunkJIT::regT1);
    
    // Pull out the butterfly from iteratedObject
    jit.load8(Address(SpecializedThunkJIT::regT0, JSCell::indexingTypeOffset()), SpecializedThunkJIT::regT3);
    jit.loadPtr(Address(SpecializedThunkJIT::regT0, JSObject::butterflyOffset()), SpecializedThunkJIT::regT2);
    Jump nullButterfly = jit.branchTestPtr(SpecializedThunkJIT::Zero, SpecializedThunkJIT::regT2);
    
    Jump notDone = jit.branch32(SpecializedThunkJIT::Below, SpecializedThunkJIT::regT1, Address(SpecializedThunkJIT::regT2, Butterfly::offsetOfPublicLength()));

    nullButterfly.link(&jit);

    // Return the termination signal to indicate that we've finished
    jit.move(TrustedImmPtr(vm->iterationTerminator.get()), SpecializedThunkJIT::regT0);
    jit.returnJSCell(SpecializedThunkJIT::regT0);
    
    notDone.link(&jit);
    
    if (kind == ArrayIterateKey) {
        jit.add32(TrustedImm32(1), Address(SpecializedThunkJIT::regT4, JSArrayIterator::offsetOfNextIndex()));
        jit.returnInt32(SpecializedThunkJIT::regT1);
        return jit.finalize(vm->jitStubs->ctiNativeTailCall(vm), "array-iterator-next-key");
        
    }
    ASSERT(kind == ArrayIterateValue);
    
    // Okay, now we're returning a value so make sure we're inside the vector size
    jit.appendFailure(jit.branch32(SpecializedThunkJIT::AboveOrEqual, SpecializedThunkJIT::regT1, Address(SpecializedThunkJIT::regT2, Butterfly::offsetOfVectorLength())));
    
    // So now we perform inline loads for int32, value/undecided, and double storage
    Jump undecidedStorage = jit.branch32(SpecializedThunkJIT::Equal, SpecializedThunkJIT::regT3, TrustedImm32(ArrayWithUndecided));
    Jump notContiguousStorage = jit.branch32(SpecializedThunkJIT::NotEqual, SpecializedThunkJIT::regT3, TrustedImm32(ArrayWithContiguous));
    
    undecidedStorage.link(&jit);
    
    jit.loadPtr(Address(SpecializedThunkJIT::regT0, JSObject::butterflyOffset()), SpecializedThunkJIT::regT2);
    
#if USE(JSVALUE64)
    jit.load64(BaseIndex(SpecializedThunkJIT::regT2, SpecializedThunkJIT::regT1, SpecializedThunkJIT::TimesEight), SpecializedThunkJIT::regT0);
    Jump notHole = jit.branchTest64(SpecializedThunkJIT::NonZero, SpecializedThunkJIT::regT0);
    jit.move(JSInterfaceJIT::TrustedImm64(ValueUndefined), JSInterfaceJIT::regT0);
    notHole.link(&jit);
    jit.addPtr(TrustedImm32(1), Address(SpecializedThunkJIT::regT4, JSArrayIterator::offsetOfNextIndex()));
    jit.returnJSValue(SpecializedThunkJIT::regT0);
#else
    jit.load32(BaseIndex(SpecializedThunkJIT::regT2, SpecializedThunkJIT::regT1, SpecializedThunkJIT::TimesEight, JSValue::offsetOfTag()), SpecializedThunkJIT::regT3);
    Jump notHole = jit.branch32(SpecializedThunkJIT::NotEqual, SpecializedThunkJIT::regT3, TrustedImm32(JSValue::EmptyValueTag));
    jit.move(JSInterfaceJIT::TrustedImm32(JSValue::UndefinedTag), JSInterfaceJIT::regT1);
    jit.move(JSInterfaceJIT::TrustedImm32(0), JSInterfaceJIT::regT0);
    jit.add32(TrustedImm32(1), Address(SpecializedThunkJIT::regT4, JSArrayIterator::offsetOfNextIndex()));
    jit.returnJSValue(SpecializedThunkJIT::regT0, JSInterfaceJIT::regT1);
    notHole.link(&jit);
    jit.load32(BaseIndex(SpecializedThunkJIT::regT2, SpecializedThunkJIT::regT1, SpecializedThunkJIT::TimesEight, JSValue::offsetOfPayload()), SpecializedThunkJIT::regT0);
    jit.add32(TrustedImm32(1), Address(SpecializedThunkJIT::regT4, JSArrayIterator::offsetOfNextIndex()));
    jit.move(SpecializedThunkJIT::regT3, SpecializedThunkJIT::regT1);
    jit.returnJSValue(SpecializedThunkJIT::regT0, SpecializedThunkJIT::regT1);
#endif
    notContiguousStorage.link(&jit);
    
    Jump notInt32Storage = jit.branch32(SpecializedThunkJIT::NotEqual, SpecializedThunkJIT::regT3, TrustedImm32(ArrayWithInt32));
    jit.loadPtr(Address(SpecializedThunkJIT::regT0, JSObject::butterflyOffset()), SpecializedThunkJIT::regT2);
    jit.load32(BaseIndex(SpecializedThunkJIT::regT2, SpecializedThunkJIT::regT1, SpecializedThunkJIT::TimesEight, JSValue::offsetOfPayload()), SpecializedThunkJIT::regT0);
    jit.add32(TrustedImm32(1), Address(SpecializedThunkJIT::regT4, JSArrayIterator::offsetOfNextIndex()));
    jit.returnInt32(SpecializedThunkJIT::regT0);
    notInt32Storage.link(&jit);
    
    jit.appendFailure(jit.branch32(SpecializedThunkJIT::NotEqual, SpecializedThunkJIT::regT3, TrustedImm32(ArrayWithDouble)));
    jit.loadPtr(Address(SpecializedThunkJIT::regT0, JSObject::butterflyOffset()), SpecializedThunkJIT::regT2);
    jit.loadDouble(BaseIndex(SpecializedThunkJIT::regT2, SpecializedThunkJIT::regT1, SpecializedThunkJIT::TimesEight), SpecializedThunkJIT::fpRegT0);
    jit.add32(TrustedImm32(1), Address(SpecializedThunkJIT::regT4, JSArrayIterator::offsetOfNextIndex()));
    jit.returnDouble(SpecializedThunkJIT::fpRegT0);
    
    return jit.finalize(vm->jitStubs->ctiNativeTailCall(vm), "array-iterator-next-value");
}

MacroAssemblerCodeRef arrayIteratorNextKeyThunkGenerator(VM* vm)
{
    return arrayIteratorNextThunkGenerator(vm, ArrayIterateKey);
}

MacroAssemblerCodeRef arrayIteratorNextValueThunkGenerator(VM* vm)
{
    return arrayIteratorNextThunkGenerator(vm, ArrayIterateValue);
}
    
}

#endif // ENABLE(JIT)
