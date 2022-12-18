/*
 * Copyright (C) 2012-2022 Apple Inc. All rights reserved.
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
#include "LLIntThunks.h"

#include "JSCJSValueInlines.h"
#include "JSInterfaceJIT.h"
#include "LLIntCLoop.h"
#include "LLIntData.h"
#include "LinkBuffer.h"
#include "VMEntryRecord.h"
#include "WasmCallingConvention.h"
#include "WasmContextInlines.h"
#include <wtf/NeverDestroyed.h>

namespace JSC {

#if ENABLE(JIT)

#if CPU(ARM64E)

JSC_ANNOTATE_JIT_OPERATION_RETURN(jitCagePtrGateAfter);
JSC_ANNOTATE_JIT_OPERATION(vmEntryToJavaScript);
JSC_ANNOTATE_JIT_OPERATION(vmEntryToNative);
JSC_ANNOTATE_JIT_OPERATION_RETURN(vmEntryToJavaScriptGateAfter);
JSC_ANNOTATE_JIT_OPERATION_RETURN(llint_function_for_call_arity_checkUntagGateAfter);
JSC_ANNOTATE_JIT_OPERATION_RETURN(llint_function_for_call_arity_checkTagGateAfter);
JSC_ANNOTATE_JIT_OPERATION_RETURN(llint_function_for_construct_arity_checkUntagGateAfter);
JSC_ANNOTATE_JIT_OPERATION_RETURN(llint_function_for_construct_arity_checkTagGateAfter);
JSC_ANNOTATE_JIT_OPERATION(vmEntryCustomGetter);
JSC_ANNOTATE_JIT_OPERATION(vmEntryCustomSetter);
JSC_ANNOTATE_JIT_OPERATION(vmEntryHostFunction);

static_assert(FunctionTraits<decltype(vmEntryCustomGetter)>::arity == FunctionTraits<GetValueFuncWithPtr>::arity, "When changing GetValueFuncWithPtr, need to change vmEntryCustomGetter implementation too.");
static_assert(FunctionTraits<decltype(vmEntryCustomSetter)>::arity == FunctionTraits<PutValueFuncWithPtr>::arity, "When changing PutValueFuncWithPtr, need to change vmEntryCustomSetter implementation too.");

#endif

namespace LLInt {

// These thunks are necessary because of nearCall used on JITed code.
// It requires that the distance from nearCall address to the destination address
// fits on 32-bits, and that's not the case of getCodeRef(llint_function_for_call_prologue)
// and others LLIntEntrypoints.

template<PtrTag tag>
static MacroAssemblerCodeRef<tag> generateThunkWithJumpTo(LLIntCode target, const char *thunkKind)
{
    JSInterfaceJIT jit;

    assertIsTaggedWith<OperationPtrTag>(target);

#if ENABLE(WEBASSEMBLY)
    CCallHelpers::RegisterID scratch = Wasm::wasmCallingConvention().prologueScratchGPRs[0];
#else
    CCallHelpers::RegisterID scratch = JSInterfaceJIT::regT0;
#endif
    jit.move(JSInterfaceJIT::TrustedImmPtr(target), scratch);
    jit.farJump(scratch, OperationPtrTag);

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::LLIntThunk);
    return FINALIZE_THUNK(patchBuffer, tag, "LLInt %s thunk", thunkKind);
}

template<PtrTag tag>
static MacroAssemblerCodeRef<tag> generateThunkWithJumpTo(OpcodeID opcodeID, const char *thunkKind)
{
    return generateThunkWithJumpTo<tag>(LLInt::getCodeFunctionPtr<OperationPtrTag>(opcodeID), thunkKind);
}

template<PtrTag tag>
static MacroAssemblerCodeRef<tag> generateThunkWithJumpToPrologue(OpcodeID opcodeID, const char *thunkKind)
{
    JSInterfaceJIT jit;

    LLIntCode target = LLInt::getCodeFunctionPtr<OperationPtrTag>(opcodeID);
    assertIsTaggedWith<OperationPtrTag>(target);

#if ENABLE(WEBASSEMBLY)
    CCallHelpers::RegisterID scratch = Wasm::wasmCallingConvention().prologueScratchGPRs[0];
#else
    CCallHelpers::RegisterID scratch = JSInterfaceJIT::regT0;
#endif
    jit.tagReturnAddress();
    jit.move(JSInterfaceJIT::TrustedImmPtr(target), scratch);
    jit.farJump(scratch, OperationPtrTag);

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::LLIntThunk);
    return FINALIZE_THUNK(patchBuffer, tag, "LLInt %s jump to prologue thunk", thunkKind);
}

template<PtrTag tag>
static MacroAssemblerCodeRef<tag> generateThunkWithJumpToLLIntReturnPoint(LLIntCode target, const char *thunkKind)
{
    JSInterfaceJIT jit;
    assertIsTaggedWith<OperationPtrTag>(target);
    jit.farJump(CCallHelpers::TrustedImmPtr(target), OperationPtrTag);
    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::LLIntThunk);
    return FINALIZE_THUNK(patchBuffer, tag, "LLInt %s return point thunk", thunkKind);
}

template<PtrTag tag>
static MacroAssemblerCodeRef<tag> generateThunkWithJumpToLLIntReturnPoint(OpcodeID opcodeID, const char *thunkKind)
{
    return generateThunkWithJumpToLLIntReturnPoint<tag>(LLInt::getCodeFunctionPtr<OperationPtrTag>(opcodeID), thunkKind);
}

MacroAssemblerCodeRef<JSEntryPtrTag> functionForCallEntryThunk()
{
    static LazyNeverDestroyed<MacroAssemblerCodeRef<JSEntryPtrTag>> codeRef;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        codeRef.construct(generateThunkWithJumpToPrologue<JSEntryPtrTag>(llint_function_for_call_prologue, "function for call"));
    });
    return codeRef;
}

MacroAssemblerCodeRef<JSEntryPtrTag> functionForConstructEntryThunk()
{
    static LazyNeverDestroyed<MacroAssemblerCodeRef<JSEntryPtrTag>> codeRef;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        codeRef.construct(generateThunkWithJumpToPrologue<JSEntryPtrTag>(llint_function_for_construct_prologue, "function for construct"));
    });
    return codeRef;
}

MacroAssemblerCodeRef<JSEntryPtrTag> functionForCallArityCheckThunk()
{
    static LazyNeverDestroyed<MacroAssemblerCodeRef<JSEntryPtrTag>> codeRef;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        codeRef.construct(generateThunkWithJumpToPrologue<JSEntryPtrTag>(llint_function_for_call_arity_check, "function for call with arity check"));
    });
    return codeRef;
}

MacroAssemblerCodeRef<JSEntryPtrTag> functionForConstructArityCheckThunk()
{
    static LazyNeverDestroyed<MacroAssemblerCodeRef<JSEntryPtrTag>> codeRef;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        codeRef.construct(generateThunkWithJumpToPrologue<JSEntryPtrTag>(llint_function_for_construct_arity_check, "function for construct with arity check"));
    });
    return codeRef;
}

MacroAssemblerCodeRef<JSEntryPtrTag> evalEntryThunk()
{
    static LazyNeverDestroyed<MacroAssemblerCodeRef<JSEntryPtrTag>> codeRef;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        codeRef.construct(generateThunkWithJumpToPrologue<JSEntryPtrTag>(llint_eval_prologue, "eval"));
    });
    return codeRef;
}

MacroAssemblerCodeRef<JSEntryPtrTag> programEntryThunk()
{
    static LazyNeverDestroyed<MacroAssemblerCodeRef<JSEntryPtrTag>> codeRef;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        codeRef.construct(generateThunkWithJumpToPrologue<JSEntryPtrTag>(llint_program_prologue, "program"));
    });
    return codeRef;
}

MacroAssemblerCodeRef<JSEntryPtrTag> moduleProgramEntryThunk()
{
    static LazyNeverDestroyed<MacroAssemblerCodeRef<JSEntryPtrTag>> codeRef;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        codeRef.construct(generateThunkWithJumpToPrologue<JSEntryPtrTag>(llint_module_program_prologue, "module_program"));
    });
    return codeRef;
}

#if ENABLE(WEBASSEMBLY)
MacroAssemblerCodeRef<JITThunkPtrTag> wasmFunctionEntryThunk()
{
    static LazyNeverDestroyed<MacroAssemblerCodeRef<JITThunkPtrTag>> codeRef;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        if (Wasm::Context::useFastTLS())
            codeRef.construct(generateThunkWithJumpToPrologue<JITThunkPtrTag>(wasm_function_prologue, "function for wasm call"));
        else
            codeRef.construct(generateThunkWithJumpToPrologue<JITThunkPtrTag>(wasm_function_prologue_no_tls, "function for wasm call"));
    });
    return codeRef;
}

MacroAssemblerCodeRef<JITThunkPtrTag> wasmFunctionEntryThunkSIMD()
{
    static LazyNeverDestroyed<MacroAssemblerCodeRef<JITThunkPtrTag>> codeRef;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        if (Wasm::Context::useFastTLS())
            codeRef.construct(generateThunkWithJumpToPrologue<JITThunkPtrTag>(wasm_function_prologue_simd, "function for wasm SIMD call"));
        else
            codeRef.construct(generateThunkWithJumpToPrologue<JITThunkPtrTag>(wasm_function_prologue_no_tls_simd, "function for wasm SIMD call"));
    });
    return codeRef;
}
#endif // ENABLE(WEBASSEMBLY)

MacroAssemblerCodeRef<JSEntryPtrTag> getHostCallReturnValueThunk()
{
    static LazyNeverDestroyed<MacroAssemblerCodeRef<JSEntryPtrTag>> codeRef;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        CCallHelpers jit;

        jit.emitFunctionPrologue();
        jit.emitGetFromCallFrameHeaderPtr(CallFrameSlot::callee, GPRInfo::regT0);

        auto preciseAllocationCase = jit.branchTestPtr(CCallHelpers::NonZero, GPRInfo::regT0, CCallHelpers::TrustedImm32(PreciseAllocation::halfAlignment));
        jit.andPtr(CCallHelpers::TrustedImmPtr(MarkedBlock::blockMask), GPRInfo::regT0);
        jit.loadPtr(CCallHelpers::Address(GPRInfo::regT0, MarkedBlock::offsetOfHeader + MarkedBlock::Header::offsetOfVM()), GPRInfo::regT0);
        auto loadedCase = jit.jump();

        preciseAllocationCase.link(&jit);
        jit.loadPtr(CCallHelpers::Address(GPRInfo::regT0, PreciseAllocation::offsetOfWeakSet() + WeakSet::offsetOfVM() - PreciseAllocation::headerSize()), GPRInfo::regT0);

        loadedCase.link(&jit);
        jit.loadValue(CCallHelpers::Address(GPRInfo::regT0, VM::offsetOfEncodedHostCallReturnValue()), JSRInfo::returnValueJSR);
        jit.emitFunctionEpilogue();
        jit.ret();

        LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::LLIntThunk);
        codeRef.construct(FINALIZE_CODE(patchBuffer, JSEntryPtrTag, "LLInt::getHostCallReturnValue thunk"));
    });
    return codeRef;
}

MacroAssemblerCodeRef<ExceptionHandlerPtrTag> callToThrowThunk()
{
    static LazyNeverDestroyed<MacroAssemblerCodeRef<ExceptionHandlerPtrTag>> codeRef;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        codeRef.construct(generateThunkWithJumpTo<ExceptionHandlerPtrTag>(llint_throw_during_call_trampoline, "LLInt::callToThrow thunk"));
    });
    return codeRef;
}

MacroAssemblerCodeRef<ExceptionHandlerPtrTag> handleUncaughtExceptionThunk()
{
    static LazyNeverDestroyed<MacroAssemblerCodeRef<ExceptionHandlerPtrTag>> codeRef;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        codeRef.construct(generateThunkWithJumpTo<ExceptionHandlerPtrTag>(llint_handle_uncaught_exception, "handle_uncaught_exception"));
    });
    return codeRef;
}

MacroAssemblerCodeRef<ExceptionHandlerPtrTag> handleCatchThunk(OpcodeSize size)
{
    switch (size) {
    case OpcodeSize::Narrow: {
        static LazyNeverDestroyed<MacroAssemblerCodeRef<ExceptionHandlerPtrTag>> codeRef;
        static std::once_flag onceKey;
        std::call_once(onceKey, [&] {
            codeRef.construct(generateThunkWithJumpTo<ExceptionHandlerPtrTag>(LLInt::getCodeFunctionPtr<OperationPtrTag>(op_catch), "op_catch"));
        });
        return codeRef;
    }
    case OpcodeSize::Wide16: {
        static LazyNeverDestroyed<MacroAssemblerCodeRef<ExceptionHandlerPtrTag>> codeRef;
        static std::once_flag onceKey;
        std::call_once(onceKey, [&] {
            codeRef.construct(generateThunkWithJumpTo<ExceptionHandlerPtrTag>(LLInt::getWide16CodeFunctionPtr<OperationPtrTag>(op_catch), "op_catch16"));
        });
        return codeRef;
    }
    case OpcodeSize::Wide32: {
        static LazyNeverDestroyed<MacroAssemblerCodeRef<ExceptionHandlerPtrTag>> codeRef;
        static std::once_flag onceKey;
        std::call_once(onceKey, [&] {
            codeRef.construct(generateThunkWithJumpTo<ExceptionHandlerPtrTag>(LLInt::getWide32CodeFunctionPtr<OperationPtrTag>(op_catch), "op_catch32"));
        });
        return codeRef;
    }
    }
    RELEASE_ASSERT_NOT_REACHED();
    return { };
}

#if ENABLE(WEBASSEMBLY)
MacroAssemblerCodeRef<ExceptionHandlerPtrTag> handleWasmCatchThunk(OpcodeSize size)
{
    WasmOpcodeID opcode = Wasm::Context::useFastTLS() ? wasm_catch : wasm_catch_no_tls;
    switch (size) {
    case OpcodeSize::Narrow: {
        static LazyNeverDestroyed<MacroAssemblerCodeRef<ExceptionHandlerPtrTag>> codeRef;
        static std::once_flag onceKey;
        std::call_once(onceKey, [&] {
            codeRef.construct(generateThunkWithJumpTo<ExceptionHandlerPtrTag>(LLInt::getCodeFunctionPtr<OperationPtrTag>(opcode), "wasm_catch"));
        });
        return codeRef;
    }
    case OpcodeSize::Wide16: {
        static LazyNeverDestroyed<MacroAssemblerCodeRef<ExceptionHandlerPtrTag>> codeRef;
        static std::once_flag onceKey;
        std::call_once(onceKey, [&] {
            codeRef.construct(generateThunkWithJumpTo<ExceptionHandlerPtrTag>(LLInt::getWide16CodeFunctionPtr<OperationPtrTag>(opcode), "wasm_catch16"));
        });
        return codeRef;
    }
    case OpcodeSize::Wide32: {
        static LazyNeverDestroyed<MacroAssemblerCodeRef<ExceptionHandlerPtrTag>> codeRef;
        static std::once_flag onceKey;
        std::call_once(onceKey, [&] {
            codeRef.construct(generateThunkWithJumpTo<ExceptionHandlerPtrTag>(LLInt::getWide32CodeFunctionPtr<OperationPtrTag>(opcode), "wasm_catch32"));
        });
        return codeRef;
    }
    }
    RELEASE_ASSERT_NOT_REACHED();
    return { };
}

MacroAssemblerCodeRef<ExceptionHandlerPtrTag> handleWasmCatchAllThunk(OpcodeSize size)
{
    WasmOpcodeID opcode = Wasm::Context::useFastTLS() ? wasm_catch_all : wasm_catch_all_no_tls;
    switch (size) {
    case OpcodeSize::Narrow: {
        static LazyNeverDestroyed<MacroAssemblerCodeRef<ExceptionHandlerPtrTag>> codeRef;
        static std::once_flag onceKey;
        std::call_once(onceKey, [&] {
            codeRef.construct(generateThunkWithJumpTo<ExceptionHandlerPtrTag>(LLInt::getCodeFunctionPtr<OperationPtrTag>(opcode), "wasm_catch_all"));
        });
        return codeRef;
    }
    case OpcodeSize::Wide16: {
        static LazyNeverDestroyed<MacroAssemblerCodeRef<ExceptionHandlerPtrTag>> codeRef;
        static std::once_flag onceKey;
        std::call_once(onceKey, [&] {
            codeRef.construct(generateThunkWithJumpTo<ExceptionHandlerPtrTag>(LLInt::getWide16CodeFunctionPtr<OperationPtrTag>(opcode), "wasm_catch_all16"));
        });
        return codeRef;
    }
    case OpcodeSize::Wide32: {
        static LazyNeverDestroyed<MacroAssemblerCodeRef<ExceptionHandlerPtrTag>> codeRef;
        static std::once_flag onceKey;
        std::call_once(onceKey, [&] {
            codeRef.construct(generateThunkWithJumpTo<ExceptionHandlerPtrTag>(LLInt::getWide32CodeFunctionPtr<OperationPtrTag>(opcode), "wasm_catch_all32"));
        });
        return codeRef;
    }
    }
    RELEASE_ASSERT_NOT_REACHED();
    return { };
}
#endif

MacroAssemblerCodeRef<JSEntryPtrTag> genericReturnPointThunk(OpcodeSize size)
{
    switch (size) {
    case OpcodeSize::Narrow: {
        static LazyNeverDestroyed<MacroAssemblerCodeRef<JSEntryPtrTag>> codeRef;
        static std::once_flag onceKey;
        std::call_once(onceKey, [&] {
            codeRef.construct(generateThunkWithJumpToLLIntReturnPoint<JSEntryPtrTag>(LLInt::getCodeFunctionPtr<OperationPtrTag>(llint_generic_return_point), "llint_generic_return_point"));
        });
        return codeRef;
    }
    case OpcodeSize::Wide16: {
        static LazyNeverDestroyed<MacroAssemblerCodeRef<JSEntryPtrTag>> codeRef;
        static std::once_flag onceKey;
        std::call_once(onceKey, [&] {
            codeRef.construct(generateThunkWithJumpToLLIntReturnPoint<JSEntryPtrTag>(LLInt::getWide16CodeFunctionPtr<OperationPtrTag>(llint_generic_return_point), "llint_generic_return_point16"));
        });
        return codeRef;
    }
    case OpcodeSize::Wide32: {
        static LazyNeverDestroyed<MacroAssemblerCodeRef<JSEntryPtrTag>> codeRef;
        static std::once_flag onceKey;
        std::call_once(onceKey, [&] {
            codeRef.construct(generateThunkWithJumpToLLIntReturnPoint<JSEntryPtrTag>(LLInt::getWide32CodeFunctionPtr<OperationPtrTag>(llint_generic_return_point), "llint_generic_return_point32"));
        });
        return codeRef;
    }
    }
    RELEASE_ASSERT_NOT_REACHED();
    return { };
}

MacroAssemblerCodeRef<JSEntryPtrTag> fuzzerReturnEarlyFromLoopHintThunk()
{
    static LazyNeverDestroyed<MacroAssemblerCodeRef<JSEntryPtrTag>> codeRef;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        codeRef.construct(generateThunkWithJumpTo<JSEntryPtrTag>(fuzzer_return_early_from_loop_hint, "fuzzer_return_early_from_loop_hint"));
    });
    return codeRef;
}

#if CPU(ARM64E)

MacroAssemblerCodeRef<NativeToJITGatePtrTag> createJSGateThunk(void* pointer, PtrTag tag, const char* name)
{
    CCallHelpers jit;

    jit.call(GPRInfo::regT5, tag);
    jit.move(CCallHelpers::TrustedImmPtr(pointer), GPRInfo::regT5);
    jit.farJump(GPRInfo::regT5, OperationPtrTag);

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::LLIntThunk);
    return FINALIZE_THUNK(patchBuffer, NativeToJITGatePtrTag, "LLInt %s call gate thunk", name);
}

MacroAssemblerCodeRef<NativeToJITGatePtrTag> createWasmGateThunk(void* pointer, PtrTag tag, const char* name)
{
    CCallHelpers jit;

    jit.call(GPRInfo::wasmScratchGPR0, tag);
    jit.move(CCallHelpers::TrustedImmPtr(pointer), GPRInfo::wasmScratchGPR1);
    jit.farJump(GPRInfo::wasmScratchGPR1, OperationPtrTag);

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::LLIntThunk);
    return FINALIZE_THUNK(patchBuffer, NativeToJITGatePtrTag, "LLInt %s wasm call gate thunk", name);
}

MacroAssemblerCodeRef<NativeToJITGatePtrTag> createTailCallGate(PtrTag tag, bool untag)
{
    CCallHelpers jit;

    if (untag) {
        jit.untagPtr(GPRInfo::argumentGPR2, ARM64Registers::lr);
        jit.validateUntaggedPtr(ARM64Registers::lr, GPRInfo::argumentGPR2);
    }
    jit.farJump(GPRInfo::argumentGPR7, tag);

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::LLIntThunk);
    return FINALIZE_THUNK(patchBuffer, NativeToJITGatePtrTag, "LLInt tail call gate thunk");
}

MacroAssemblerCodeRef<NativeToJITGatePtrTag> createWasmTailCallGate(PtrTag tag)
{
    CCallHelpers jit;

    jit.untagPtr(GPRInfo::wasmScratchGPR2, ARM64Registers::lr);
    jit.validateUntaggedPtr(ARM64Registers::lr, GPRInfo::wasmScratchGPR2);
    jit.farJump(GPRInfo::wasmScratchGPR0, tag);

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::LLIntThunk);
    return FINALIZE_THUNK(patchBuffer, NativeToJITGatePtrTag, "LLInt wasm tail call gate thunk");
}

MacroAssemblerCodeRef<NativeToJITGatePtrTag> loopOSREntryGateThunk()
{
    static LazyNeverDestroyed<MacroAssemblerCodeRef<NativeToJITGatePtrTag>> codeRef;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        CCallHelpers jit;

        jit.farJump(GPRInfo::argumentGPR0, JSEntryPtrTag);

        LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::LLIntThunk);
        codeRef.construct(FINALIZE_CODE(patchBuffer, NativeToJITGatePtrTag, "loop OSR entry thunk"));
    });
    return codeRef;
}

MacroAssemblerCodeRef<NativeToJITGatePtrTag> entryOSREntryGateThunk()
{
    static LazyNeverDestroyed<MacroAssemblerCodeRef<NativeToJITGatePtrTag>> codeRef;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        CCallHelpers jit;

        jit.untagReturnAddress();
        jit.farJump(GPRInfo::argumentGPR0, JSEntryPtrTag);

        LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::LLIntThunk);
        codeRef.construct(FINALIZE_CODE(patchBuffer, NativeToJITGatePtrTag, "entry OSR entry thunk"));
    });
    return codeRef;
}

MacroAssemblerCodeRef<NativeToJITGatePtrTag> wasmOSREntryGateThunk()
{
    static LazyNeverDestroyed<MacroAssemblerCodeRef<NativeToJITGatePtrTag>> codeRef;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        CCallHelpers jit;

        jit.untagReturnAddress();
        jit.farJump(GPRInfo::wasmScratchGPR0, WasmEntryPtrTag);

        LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::LLIntThunk);
        codeRef.construct(FINALIZE_CODE(patchBuffer, NativeToJITGatePtrTag, "wasm OSR entry thunk"));
    });
    return codeRef;
}

MacroAssemblerCodeRef<NativeToJITGatePtrTag> exceptionHandlerGateThunk()
{
    static LazyNeverDestroyed<MacroAssemblerCodeRef<NativeToJITGatePtrTag>> codeRef;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        CCallHelpers jit;

        jit.farJump(GPRInfo::regT0, ExceptionHandlerPtrTag);

        LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::LLIntThunk);
        codeRef.construct(FINALIZE_CODE(patchBuffer, NativeToJITGatePtrTag, "exception handler thunk"));
    });
    return codeRef;
}

MacroAssemblerCodeRef<NativeToJITGatePtrTag> returnFromLLIntGateThunk()
{
    static LazyNeverDestroyed<MacroAssemblerCodeRef<NativeToJITGatePtrTag>> codeRef;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        CCallHelpers jit;

        jit.ret();

        LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::LLIntThunk);
        codeRef.construct(FINALIZE_CODE(patchBuffer, NativeToJITGatePtrTag, "returnFromLLInt thunk"));
    });
    return codeRef;
}

MacroAssemblerCodeRef<NativeToJITGatePtrTag> tagGateThunk(void* pointer)
{
    CCallHelpers jit;

    jit.addPtr(CCallHelpers::TrustedImm32(16), GPRInfo::callFrameRegister, GPRInfo::regT3);
    jit.tagPtr(GPRInfo::regT3, ARM64Registers::lr);
    jit.storePtr(ARM64Registers::lr, CCallHelpers::Address(GPRInfo::callFrameRegister, 8));
    jit.move(CCallHelpers::TrustedImmPtr(pointer), GPRInfo::regT3);
    jit.farJump(GPRInfo::regT3, OperationPtrTag);

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::LLIntThunk);
    return FINALIZE_THUNK(patchBuffer, NativeToJITGatePtrTag, "tag thunk");
}

MacroAssemblerCodeRef<NativeToJITGatePtrTag> untagGateThunk(void* pointer)
{
    CCallHelpers jit;

    jit.loadPtr(CCallHelpers::Address(GPRInfo::callFrameRegister, 8), ARM64Registers::lr);
    jit.addPtr(CCallHelpers::TrustedImm32(16), GPRInfo::callFrameRegister, GPRInfo::regT3);
    jit.untagPtr(GPRInfo::regT3, ARM64Registers::lr);
    jit.validateUntaggedPtr(ARM64Registers::lr, GPRInfo::regT3);
    jit.move(CCallHelpers::TrustedImmPtr(pointer), GPRInfo::regT3);
    jit.farJump(GPRInfo::regT3, OperationPtrTag);

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::LLIntThunk);
    return FINALIZE_THUNK(patchBuffer, NativeToJITGatePtrTag, "untag thunk");
}

#endif // CPU(ARM64E)

#if ENABLE(JIT_CAGE)

MacroAssemblerCodeRef<NativeToJITGatePtrTag> jitCagePtrThunk()
{
    static LazyNeverDestroyed<MacroAssemblerCodeRef<NativeToJITGatePtrTag>> codeRef;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        CCallHelpers jit;
        JSC_JIT_CAGE_COMPILE_IMPL(jit);
        LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::LLIntThunk);
        codeRef.construct(FINALIZE_CODE(patchBuffer, NativeToJITGatePtrTag, "jitCagePtr thunk"));
    });
    return codeRef;
}

#endif // ENABLE(JIT_CAGE)

MacroAssemblerCodeRef<JSEntryPtrTag> normalOSRExitTrampolineThunk()
{
    static LazyNeverDestroyed<MacroAssemblerCodeRef<JSEntryPtrTag>> codeRef;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        codeRef.construct(generateThunkWithJumpToLLIntReturnPoint<JSEntryPtrTag>(normal_osr_exit_trampoline, "normal_osr_exit_trampoline thunk"));
    });
    return codeRef;
}

#if ENABLE(DFG_JIT)

MacroAssemblerCodeRef<JSEntryPtrTag> checkpointOSRExitTrampolineThunk()
{
    static LazyNeverDestroyed<MacroAssemblerCodeRef<JSEntryPtrTag>> codeRef;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        codeRef.construct(generateThunkWithJumpToLLIntReturnPoint<JSEntryPtrTag>(checkpoint_osr_exit_trampoline, "checkpoint_osr_exit_trampoline thunk"));
    });
    return codeRef;
}

MacroAssemblerCodeRef<JSEntryPtrTag> checkpointOSRExitFromInlinedCallTrampolineThunk()
{
    static LazyNeverDestroyed<MacroAssemblerCodeRef<JSEntryPtrTag>> codeRef;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        codeRef.construct(generateThunkWithJumpToLLIntReturnPoint<JSEntryPtrTag>(checkpoint_osr_exit_from_inlined_call_trampoline, "checkpoint_osr_exit_from_inlined_call_trampoline thunk"));
    });
    return codeRef;
}

MacroAssemblerCodeRef<JSEntryPtrTag> returnLocationThunk(OpcodeID opcodeID, OpcodeSize size)
{
#define LLINT_RETURN_LOCATION(name) \
    case name##_return_location: { \
        switch (size) { \
        case OpcodeSize::Narrow: { \
            static LazyNeverDestroyed<MacroAssemblerCodeRef<JSEntryPtrTag>> codeRef; \
            static std::once_flag onceKey; \
            std::call_once(onceKey, [&] { \
                codeRef.construct(generateThunkWithJumpToLLIntReturnPoint<JSEntryPtrTag>(LLInt::getCodeFunctionPtr<OperationPtrTag>(name##_return_location),  #name "_return_location thunk")); \
            }); \
            return codeRef; \
        } \
        case OpcodeSize::Wide16: { \
            static LazyNeverDestroyed<MacroAssemblerCodeRef<JSEntryPtrTag>> codeRef; \
            static std::once_flag onceKey; \
            std::call_once(onceKey, [&] { \
                codeRef.construct(generateThunkWithJumpToLLIntReturnPoint<JSEntryPtrTag>(LLInt::getWide16CodeFunctionPtr<OperationPtrTag>(name##_return_location),  #name "_return_location16 thunk")); \
            }); \
            return codeRef; \
        } \
        case OpcodeSize::Wide32: { \
            static LazyNeverDestroyed<MacroAssemblerCodeRef<JSEntryPtrTag>> codeRef; \
            static std::once_flag onceKey; \
            std::call_once(onceKey, [&] { \
                codeRef.construct(generateThunkWithJumpToLLIntReturnPoint<JSEntryPtrTag>(LLInt::getWide32CodeFunctionPtr<OperationPtrTag>(name##_return_location), #name "_return_location32 thunk")); \
            }); \
            return codeRef; \
        } \
        } \
        return { }; \
    }

    switch (opcodeID) {
    LLINT_RETURN_LOCATION(op_call)
    LLINT_RETURN_LOCATION(op_iterator_open)
    LLINT_RETURN_LOCATION(op_iterator_next)
    LLINT_RETURN_LOCATION(op_construct)
    LLINT_RETURN_LOCATION(op_call_varargs)
    LLINT_RETURN_LOCATION(op_construct_varargs)
    LLINT_RETURN_LOCATION(op_get_by_id)
    LLINT_RETURN_LOCATION(op_get_by_val)
    LLINT_RETURN_LOCATION(op_put_by_id)
    LLINT_RETURN_LOCATION(op_put_by_val)
    default:
        RELEASE_ASSERT_NOT_REACHED();
        return { };
    }
}

#endif

} // namespace LLInt

#endif // ENABLE(JIT)

#if ENABLE(C_LOOP)
// Non-JIT (i.e. C Loop LLINT) case:

EncodedJSValue vmEntryToJavaScript(void* executableAddress, VM* vm, ProtoCallFrame* protoCallFrame)
{
    JSValue result = CLoop::execute(llint_vm_entry_to_javascript, executableAddress, vm, protoCallFrame);
    return JSValue::encode(result);
}

EncodedJSValue vmEntryToNative(void* executableAddress, VM* vm, ProtoCallFrame* protoCallFrame)
{
    JSValue result = CLoop::execute(llint_vm_entry_to_native, executableAddress, vm, protoCallFrame);
    return JSValue::encode(result);
}

extern "C" VMEntryRecord* vmEntryRecord(EntryFrame* entryFrame)
{
    // The C Loop doesn't have any callee save registers, so the VMEntryRecord is allocated at the base of the frame.
    intptr_t stackAlignment = stackAlignmentBytes();
    intptr_t VMEntryTotalFrameSize = (sizeof(VMEntryRecord) + (stackAlignment - 1)) & ~(stackAlignment - 1);
    return reinterpret_cast<VMEntryRecord*>(reinterpret_cast<char*>(entryFrame) - VMEntryTotalFrameSize);
}

#endif // ENABLE(C_LOOP)

} // namespace JSC
