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

#include "InPlaceInterpreter.h"
#include "JSCJSValueInlines.h"
#include "JSInterfaceJIT.h"
#include "LLIntCLoop.h"
#include "LLIntData.h"
#include "LinkBuffer.h"
#include "MaxFrameExtentForSlowPathCall.h"
#include "VMEntryRecord.h"
#include "WasmCallingConvention.h"
#include "WasmContext.h"
#include <wtf/NeverDestroyed.h>

namespace JSC {

#if ENABLE(JIT)

#if CPU(ARM64E)

JSC_ANNOTATE_JIT_OPERATION_RETURN(jitCagePtrGateAfter);
JSC_ANNOTATE_JIT_OPERATION(vmEntryToJavaScript);
JSC_ANNOTATE_JIT_OPERATION(vmEntryToJavaScriptWith0Arguments);
JSC_ANNOTATE_JIT_OPERATION(vmEntryToJavaScriptWith1Arguments);
JSC_ANNOTATE_JIT_OPERATION(vmEntryToJavaScriptWith2Arguments);
JSC_ANNOTATE_JIT_OPERATION(vmEntryToJavaScriptWith3Arguments);
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
    return FINALIZE_THUNK(patchBuffer, tag, ASCIILiteral::fromLiteralUnsafe(thunkKind), "LLInt %s thunk", thunkKind);
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
    return FINALIZE_THUNK(patchBuffer, tag, ASCIILiteral::fromLiteralUnsafe(thunkKind), "LLInt %s jump to prologue thunk", thunkKind);
}

template<PtrTag tag>
static MacroAssemblerCodeRef<tag> generateThunkWithJumpToLLIntReturnPoint(LLIntCode target, const char *thunkKind)
{
    JSInterfaceJIT jit;
    assertIsTaggedWith<OperationPtrTag>(target);
    jit.farJump(CCallHelpers::TrustedImmPtr(target), OperationPtrTag);
    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::LLIntThunk);
    return FINALIZE_THUNK(patchBuffer, tag, ASCIILiteral::fromLiteralUnsafe(thunkKind), "LLInt %s return point thunk", thunkKind);
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
        codeRef.construct(generateThunkWithJumpToPrologue<JITThunkPtrTag>(wasm_function_prologue, "function for wasm call"));
    });
    return codeRef;
}

MacroAssemblerCodeRef<JITThunkPtrTag> wasmFunctionEntryThunkSIMD()
{
    static LazyNeverDestroyed<MacroAssemblerCodeRef<JITThunkPtrTag>> codeRef;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        codeRef.construct(generateThunkWithJumpToPrologue<JITThunkPtrTag>(wasm_function_prologue_simd, "function for wasm SIMD call"));
    });
    return codeRef;
}


ALWAYS_INLINE void* untaggedPtr(void* ptr)
{
    return CodePtr<CFunctionPtrTag>::fromTaggedPtr(ptr).template untaggedPtr<>();
}

MacroAssemblerCodeRef<JITThunkPtrTag> inPlaceInterpreterEntryThunk()
{
    static LazyNeverDestroyed<MacroAssemblerCodeRef<JITThunkPtrTag>> codeRef;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        codeRef.construct(generateThunkWithJumpToPrologue<JITThunkPtrTag>(ipint_entry, "function for IPInt entry"));
    });
    return codeRef;
}
#endif // ENABLE(WEBASSEMBLY)

MacroAssemblerCodeRef<JSEntryPtrTag> defaultCallThunk()
{
    static LazyNeverDestroyed<MacroAssemblerCodeRef<JSEntryPtrTag>> codeRef;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        // The callee is in regT0 (for JSVALUE32_64, the tag is in regT1).
        // The return address is on the stack, or in the link register. We will hence
        // jump to the callee, or save the return address to the call frame while we
        // make a C++ function call to the appropriate JIT operation.

        // regT0 => callee
        // regT1 => tag (32bit)
        // regT2 => CallLinkInfo*

        CCallHelpers jit;

        jit.emitFunctionPrologue();
        if (maxFrameExtentForSlowPathCall)
            jit.addPtr(CCallHelpers::TrustedImm32(-static_cast<int32_t>(maxFrameExtentForSlowPathCall)), CCallHelpers::stackPointerRegister);
        jit.setupArguments<decltype(operationDefaultCall)>(GPRInfo::regT2);
        jit.move(CCallHelpers::TrustedImmPtr(tagCFunction<OperationPtrTag>(operationDefaultCall)), GPRInfo::nonArgGPR0);
        jit.call(GPRInfo::nonArgGPR0, OperationPtrTag);
        if (maxFrameExtentForSlowPathCall)
            jit.addPtr(CCallHelpers::TrustedImm32(maxFrameExtentForSlowPathCall), CCallHelpers::stackPointerRegister);

        // This slow call will return the address of one of the following:
        // 1) Exception throwing thunk.
        // 2) Host call return value returner thingy.
        // 3) The function to call.
        // The second return value GPR will hold a non-zero value for tail calls.

        jit.emitFunctionEpilogue();
        jit.untagReturnAddress();
        jit.farJump(GPRInfo::returnValueGPR, JSEntryPtrTag);

        LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::Thunk);
        codeRef.construct(FINALIZE_CODE(patchBuffer, JSEntryPtrTag, "DefaultCall"_s, "Default Call thunk"));
        return;
    });
    return codeRef;
}

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
        codeRef.construct(FINALIZE_CODE(patchBuffer, JSEntryPtrTag, "getHostCallReturnValue"_s, "LLInt::getHostCallReturnValue thunk"));
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
    WasmOpcodeID opcode = wasm_catch;
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
    WasmOpcodeID opcode = wasm_catch_all;
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

MacroAssemblerCodeRef<ExceptionHandlerPtrTag> handleWasmTryTableThunk(WasmOpcodeID opcode, OpcodeSize size)
{
    switch (size) {
    case OpcodeSize::Narrow: {
        static LazyNeverDestroyed<MacroAssemblerCodeRef<ExceptionHandlerPtrTag>> codeRefCatch;
        static LazyNeverDestroyed<MacroAssemblerCodeRef<ExceptionHandlerPtrTag>> codeRefCatchRef;
        static LazyNeverDestroyed<MacroAssemblerCodeRef<ExceptionHandlerPtrTag>> codeRefCatchAll;
        static LazyNeverDestroyed<MacroAssemblerCodeRef<ExceptionHandlerPtrTag>> codeRefCatchAllRef;
        static std::once_flag onceKey;
        std::call_once(onceKey, [&] {
            codeRefCatch.construct(generateThunkWithJumpTo<ExceptionHandlerPtrTag>(LLInt::getCodeFunctionPtr<OperationPtrTag>(wasm_try_table_catch), "wasm_try_table_catch"));
            codeRefCatchRef.construct(generateThunkWithJumpTo<ExceptionHandlerPtrTag>(LLInt::getCodeFunctionPtr<OperationPtrTag>(wasm_try_table_catchref), "wasm_try_table_catchref"));
            codeRefCatchAll.construct(generateThunkWithJumpTo<ExceptionHandlerPtrTag>(LLInt::getCodeFunctionPtr<OperationPtrTag>(wasm_try_table_catchall), "wasm_try_table_catchall"));
            codeRefCatchAllRef.construct(generateThunkWithJumpTo<ExceptionHandlerPtrTag>(LLInt::getCodeFunctionPtr<OperationPtrTag>(wasm_try_table_catchallref), "wasm_try_table_catchallref"));
        });
        switch (opcode) {
        case wasm_try_table_catch:
            return codeRefCatch;
        case wasm_try_table_catchref:
            return codeRefCatchRef;
        case wasm_try_table_catchall:
            return codeRefCatchAll;
        case wasm_try_table_catchallref:
            return codeRefCatchAllRef;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }
    case OpcodeSize::Wide16: {
        static LazyNeverDestroyed<MacroAssemblerCodeRef<ExceptionHandlerPtrTag>> codeRefCatch;
        static LazyNeverDestroyed<MacroAssemblerCodeRef<ExceptionHandlerPtrTag>> codeRefCatchRef;
        static LazyNeverDestroyed<MacroAssemblerCodeRef<ExceptionHandlerPtrTag>> codeRefCatchAll;
        static LazyNeverDestroyed<MacroAssemblerCodeRef<ExceptionHandlerPtrTag>> codeRefCatchAllRef;
        static std::once_flag onceKey;
        std::call_once(onceKey, [&] {
            codeRefCatch.construct(generateThunkWithJumpTo<ExceptionHandlerPtrTag>(LLInt::getWide16CodeFunctionPtr<OperationPtrTag>(wasm_try_table_catch), "wasm_try_table_catch16"));
            codeRefCatchRef.construct(generateThunkWithJumpTo<ExceptionHandlerPtrTag>(LLInt::getWide16CodeFunctionPtr<OperationPtrTag>(wasm_try_table_catchref), "wasm_try_table_catchref16"));
            codeRefCatchAll.construct(generateThunkWithJumpTo<ExceptionHandlerPtrTag>(LLInt::getWide16CodeFunctionPtr<OperationPtrTag>(wasm_try_table_catchall), "wasm_try_table_catchall16"));
            codeRefCatchAllRef.construct(generateThunkWithJumpTo<ExceptionHandlerPtrTag>(LLInt::getWide16CodeFunctionPtr<OperationPtrTag>(wasm_try_table_catchallref), "wasm_try_table_catchallref16"));
        });
        switch (opcode) {
        case wasm_try_table_catch:
            return codeRefCatch;
        case wasm_try_table_catchref:
            return codeRefCatchRef;
        case wasm_try_table_catchall:
            return codeRefCatchAll;
        case wasm_try_table_catchallref:
            return codeRefCatchAllRef;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }
    case OpcodeSize::Wide32: {
        static LazyNeverDestroyed<MacroAssemblerCodeRef<ExceptionHandlerPtrTag>> codeRefCatch;
        static LazyNeverDestroyed<MacroAssemblerCodeRef<ExceptionHandlerPtrTag>> codeRefCatchRef;
        static LazyNeverDestroyed<MacroAssemblerCodeRef<ExceptionHandlerPtrTag>> codeRefCatchAll;
        static LazyNeverDestroyed<MacroAssemblerCodeRef<ExceptionHandlerPtrTag>> codeRefCatchAllRef;
        static std::once_flag onceKey;
        std::call_once(onceKey, [&] {
            codeRefCatch.construct(generateThunkWithJumpTo<ExceptionHandlerPtrTag>(LLInt::getWide32CodeFunctionPtr<OperationPtrTag>(wasm_try_table_catch), "wasm_try_table_catch32"));
            codeRefCatchRef.construct(generateThunkWithJumpTo<ExceptionHandlerPtrTag>(LLInt::getWide32CodeFunctionPtr<OperationPtrTag>(wasm_try_table_catchref), "wasm_try_table_catchref32"));
            codeRefCatchAll.construct(generateThunkWithJumpTo<ExceptionHandlerPtrTag>(LLInt::getWide32CodeFunctionPtr<OperationPtrTag>(wasm_try_table_catchall), "wasm_try_table_catchall32"));
            codeRefCatchAllRef.construct(generateThunkWithJumpTo<ExceptionHandlerPtrTag>(LLInt::getWide32CodeFunctionPtr<OperationPtrTag>(wasm_try_table_catchallref), "wasm_try_table_catchallref32"));
        });
        switch (opcode) {
        case wasm_try_table_catch:
            return codeRefCatch;
        case wasm_try_table_catchref:
            return codeRefCatchRef;
        case wasm_try_table_catchall:
            return codeRefCatchAll;
        case wasm_try_table_catchallref:
            return codeRefCatchAllRef;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
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

#if ENABLE(JIT)
MacroAssemblerCodeRef<JITThunkPtrTag> arityFixupThunk()
{
    static LazyNeverDestroyed<MacroAssemblerCodeRef<JITThunkPtrTag>> codeRef;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&]{
        CCallHelpers jit;

        // We enter with fixup count in argumentGPR0
        // Caller's linkRegister in argumentGPR1
        // argumentCountIncludingThis in argumentGPR2
        // We have the guarantee that a0, a1, a2, t3, t4 and t5 (or t0 for Windows) are all distinct :-)
#if USE(JSVALUE64)
        static_assert(noOverlap(GPRInfo::argumentGPR0, GPRInfo::argumentGPR1, GPRInfo::argumentGPR2, GPRInfo::regT3, GPRInfo::regT4, GPRInfo::regT5));
#if CPU(X86_64)
        jit.pop(JSInterfaceJIT::regT4);
#endif
        jit.subPtr(JSInterfaceJIT::stackPointerRegister, CCallHelpers::TrustedImm32(static_cast<int32_t>(sizeof(CallerFrameAndPC))), JSInterfaceJIT::regT3); // Initially expected callFramePointer after prologue.
        jit.add32(JSInterfaceJIT::TrustedImm32(CallFrame::headerSizeInRegisters), JSInterfaceJIT::argumentGPR2);

        // Check to see if we have extra slots we can use
        //
        // argumentGPR0's padding is `align2(numParameters + CallFrame::headerSizeInRegisters) - (argumentCountIncludingThis + CallFrame::headerSizeInRegisters)`
        // And extra slot means the above padding is an odd number. And after filling extra slot, it becomes +1, thus even number.
        JSInterfaceJIT::Jump noExtraSlot = jit.branchTest32(MacroAssembler::Zero, JSInterfaceJIT::argumentGPR0, JSInterfaceJIT::TrustedImm32(stackAlignmentRegisters() - 1));
        jit.move(JSInterfaceJIT::TrustedImm64(JSValue::ValueUndefined), GPRInfo::regT5);
        static_assert(stackAlignmentRegisters() == 2);
        jit.store64(GPRInfo::regT5, MacroAssembler::BaseIndex(JSInterfaceJIT::regT3, JSInterfaceJIT::argumentGPR2, JSInterfaceJIT::TimesEight));
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
        jit.lshift64(JSInterfaceJIT::argumentGPR0, JSInterfaceJIT::TrustedImm32(3), GPRInfo::regT5);
        jit.addPtr(GPRInfo::regT5, JSInterfaceJIT::stackPointerRegister);

        // Move current frame down argumentGPR0 number of slots
#if CPU(ARM64)
        jit.addPtr(JSInterfaceJIT::regT3, GPRInfo::regT5);
        JSInterfaceJIT::Label copyLoop(jit.label());
        jit.loadPair64(CCallHelpers::PostIndexAddress(JSInterfaceJIT::regT3, 16), GPRInfo::regT7, GPRInfo::regT6);
        jit.storePair64(GPRInfo::regT7, GPRInfo::regT6, CCallHelpers::PostIndexAddress(GPRInfo::regT5, 16));
        jit.branchSub32(MacroAssembler::NonZero, JSInterfaceJIT::TrustedImm32(2), JSInterfaceJIT::argumentGPR2).linkTo(copyLoop, &jit);

        // Fill in argumentGPR0 missing arg slots with undefined
        jit.move(JSInterfaceJIT::TrustedImm64(JSValue::ValueUndefined), GPRInfo::regT7);
        JSInterfaceJIT::Label fillUndefinedLoop(jit.label());
        jit.storePair64(GPRInfo::regT7, GPRInfo::regT7, CCallHelpers::PostIndexAddress(GPRInfo::regT5, 16));
        jit.branchAdd32(MacroAssembler::NonZero, JSInterfaceJIT::TrustedImm32(2), JSInterfaceJIT::argumentGPR0).linkTo(fillUndefinedLoop, &jit);
#else
        JSInterfaceJIT::Label copyLoop(jit.label());
        jit.load64(CCallHelpers::Address(JSInterfaceJIT::regT3), GPRInfo::regT5);
        jit.store64(GPRInfo::regT5, MacroAssembler::BaseIndex(JSInterfaceJIT::regT3, JSInterfaceJIT::argumentGPR0, JSInterfaceJIT::TimesEight));
        jit.addPtr(JSInterfaceJIT::TrustedImm32(8), JSInterfaceJIT::regT3);
        jit.branchSub32(MacroAssembler::NonZero, JSInterfaceJIT::TrustedImm32(1), JSInterfaceJIT::argumentGPR2).linkTo(copyLoop, &jit);

        // Fill in argumentGPR0 missing arg slots with undefined
        jit.move(JSInterfaceJIT::argumentGPR0, JSInterfaceJIT::argumentGPR2);
        jit.move(JSInterfaceJIT::TrustedImm64(JSValue::ValueUndefined), GPRInfo::regT5);
        JSInterfaceJIT::Label fillUndefinedLoop(jit.label());
        jit.store64(GPRInfo::regT5, MacroAssembler::BaseIndex(JSInterfaceJIT::regT3, JSInterfaceJIT::argumentGPR0, JSInterfaceJIT::TimesEight));
        jit.addPtr(JSInterfaceJIT::TrustedImm32(8), JSInterfaceJIT::regT3);
        jit.branchAdd32(MacroAssembler::NonZero, JSInterfaceJIT::TrustedImm32(1), JSInterfaceJIT::argumentGPR2).linkTo(fillUndefinedLoop, &jit);
#endif

        done.link(&jit);
        jit.tagReturnAddress();
#if CPU(X86_64)
        jit.push(JSInterfaceJIT::regT4);
#endif
        jit.ret();

#else // USE(JSVALUE64) section above, USE(JSVALUE32_64) section below.
        jit.subPtr(JSInterfaceJIT::stackPointerRegister, CCallHelpers::TrustedImm32(static_cast<int32_t>(sizeof(CallerFrameAndPC))), JSInterfaceJIT::regT3); // Initially expected callFramePointer after prologue.
        jit.add32(JSInterfaceJIT::TrustedImm32(CallFrame::headerSizeInRegisters), JSInterfaceJIT::argumentGPR2);

        // Check to see if we have extra slots we can use
        jit.move(JSInterfaceJIT::argumentGPR0, GPRInfo::regT4);
        jit.and32(JSInterfaceJIT::TrustedImm32(stackAlignmentRegisters() - 1), GPRInfo::regT4);
        JSInterfaceJIT::Jump noExtraSlot = jit.branchTest32(MacroAssembler::Zero, GPRInfo::regT4);
        JSInterfaceJIT::Label fillExtraSlots(jit.label());
        jit.move(JSInterfaceJIT::TrustedImm32(0), JSInterfaceJIT::regT5);
        jit.store32(JSInterfaceJIT::regT5, MacroAssembler::BaseIndex(JSInterfaceJIT::regT3, JSInterfaceJIT::argumentGPR2, JSInterfaceJIT::TimesEight, PayloadOffset));
        jit.move(JSInterfaceJIT::TrustedImm32(JSValue::UndefinedTag), JSInterfaceJIT::regT5);
        jit.store32(JSInterfaceJIT::regT5, MacroAssembler::BaseIndex(JSInterfaceJIT::regT3, JSInterfaceJIT::argumentGPR2, JSInterfaceJIT::TimesEight, TagOffset));
        jit.add32(JSInterfaceJIT::TrustedImm32(1), JSInterfaceJIT::argumentGPR2);
        jit.branchSub32(JSInterfaceJIT::NonZero, JSInterfaceJIT::TrustedImm32(1), GPRInfo::regT4).linkTo(fillExtraSlots, &jit);
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
        jit.addPtr(JSInterfaceJIT::regT5, JSInterfaceJIT::stackPointerRegister);

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

        jit.tagReturnAddress();
        jit.ret();
#endif // End of USE(JSVALUE32_64) section.

        LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::LLIntThunk);
        codeRef.construct(FINALIZE_CODE(patchBuffer, JITThunkPtrTag, "arityFixup"_s, "fixup arity"));
    });
    return codeRef;
}
#endif

#if CPU(ARM64E)

MacroAssemblerCodeRef<NativeToJITGatePtrTag> createJSGateThunk(void* pointer, PtrTag tag, const char* name)
{
    CCallHelpers jit;

    jit.call(GPRInfo::regT5, tag);
    jit.move(CCallHelpers::TrustedImmPtr(pointer), GPRInfo::regT5);
    jit.farJump(GPRInfo::regT5, OperationPtrTag);

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::LLIntThunk);
    return FINALIZE_THUNK(patchBuffer, NativeToJITGatePtrTag, "CallJSGate"_s, "LLInt %s call gate thunk", name);
}

MacroAssemblerCodeRef<NativeToJITGatePtrTag> createWasmGateThunk(void* pointer, PtrTag tag, const char* name)
{
    CCallHelpers jit;

    jit.call(GPRInfo::wasmScratchGPR0, tag);
    jit.move(CCallHelpers::TrustedImmPtr(pointer), GPRInfo::wasmScratchGPR1);
    jit.farJump(GPRInfo::wasmScratchGPR1, OperationPtrTag);

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::LLIntThunk);
    return FINALIZE_THUNK(patchBuffer, NativeToJITGatePtrTag, "CallWasmGate"_s, "LLInt %s wasm call gate thunk", name);
}

MacroAssemblerCodeRef<NativeToJITGatePtrTag> createTailCallGate(PtrTag tag, bool untag)
{
    CCallHelpers jit;

    if (untag) {
        jit.untagPtr(GPRInfo::argumentGPR6, ARM64Registers::lr);
        jit.validateUntaggedPtr(ARM64Registers::lr, GPRInfo::argumentGPR6);
    }
    jit.farJump(GPRInfo::argumentGPR7, tag);

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::LLIntThunk);
    return FINALIZE_THUNK(patchBuffer, NativeToJITGatePtrTag, "TailCallJSGate"_s, "LLInt tail call gate thunk");
}

MacroAssemblerCodeRef<NativeToJITGatePtrTag> createWasmTailCallGate(PtrTag tag)
{
    CCallHelpers jit;

    jit.untagPtr(GPRInfo::wasmScratchGPR2, ARM64Registers::lr);
    jit.validateUntaggedPtr(ARM64Registers::lr, GPRInfo::wasmScratchGPR2);
    jit.farJump(GPRInfo::wasmScratchGPR0, tag);

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::LLIntThunk);
    return FINALIZE_THUNK(patchBuffer, NativeToJITGatePtrTag, "TailCallWasmGate"_s, "LLInt wasm tail call gate thunk");
}

MacroAssemblerCodeRef<NativeToJITGatePtrTag> loopOSREntryGateThunk()
{
    static LazyNeverDestroyed<MacroAssemblerCodeRef<NativeToJITGatePtrTag>> codeRef;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        CCallHelpers jit;

        jit.farJump(GPRInfo::argumentGPR0, JSEntryPtrTag);

        LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::LLIntThunk);
        codeRef.construct(FINALIZE_CODE(patchBuffer, NativeToJITGatePtrTag, "LoopOSREntry"_s, "loop OSR entry thunk"));
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
        codeRef.construct(FINALIZE_CODE(patchBuffer, NativeToJITGatePtrTag, "OSREntry"_s, "entry OSR entry thunk"));
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
        codeRef.construct(FINALIZE_CODE(patchBuffer, NativeToJITGatePtrTag, "WasmOSREntry"_s, "wasm OSR entry thunk"));
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
        codeRef.construct(FINALIZE_CODE(patchBuffer, NativeToJITGatePtrTag, "exceptionHandler"_s, "exception handler thunk"));
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
        codeRef.construct(FINALIZE_CODE(patchBuffer, NativeToJITGatePtrTag, "returnFromLLInt"_s, "returnFromLLInt thunk"));
    });
    return codeRef;
}

MacroAssemblerCodeRef<NativeToJITGatePtrTag> tagGateThunk(void* pointer)
{
    CCallHelpers jit;

    GPRReg signingTagReg = GPRInfo::regT3;
    if (!Options::allowNonSPTagging()) {
        JIT_COMMENT(jit, "lldb dynamic execution / posix signals could trash your stack"); // We don't have to worry about signals because they shouldn't fire in WebContent process in this window.
        jit.move(MacroAssembler::stackPointerRegister, GPRInfo::regT3);
        signingTagReg = MacroAssembler::stackPointerRegister;
    }

    jit.addPtr(CCallHelpers::TrustedImm32(sizeof(CallerFrameAndPC)), GPRInfo::callFrameRegister, signingTagReg);
    jit.tagPtr(signingTagReg, ARM64Registers::lr);
    jit.storePtr(ARM64Registers::lr, CCallHelpers::Address(GPRInfo::callFrameRegister, sizeof(CPURegister)));

    if (!Options::allowNonSPTagging()) {
        JIT_COMMENT(jit, "lldb dynamic execution / posix signals are ok again");
        jit.move(GPRInfo::regT3, MacroAssembler::stackPointerRegister);
    }

    jit.move(CCallHelpers::TrustedImmPtr(pointer), GPRInfo::regT3);
    jit.farJump(GPRInfo::regT3, OperationPtrTag);

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::LLIntThunk);
    return FINALIZE_THUNK(patchBuffer, NativeToJITGatePtrTag, "tag"_s, "tag thunk");
}

MacroAssemblerCodeRef<NativeToJITGatePtrTag> untagGateThunk(void* pointer)
{
    CCallHelpers jit;

    jit.loadPtr(CCallHelpers::Address(GPRInfo::callFrameRegister, sizeof(CPURegister)), ARM64Registers::lr);
    jit.addPtr(CCallHelpers::TrustedImm32(sizeof(CallerFrameAndPC)), GPRInfo::callFrameRegister, GPRInfo::regT3);
    jit.untagPtr(GPRInfo::regT3, ARM64Registers::lr);
    jit.validateUntaggedPtr(ARM64Registers::lr, GPRInfo::regT3);
    jit.move(CCallHelpers::TrustedImmPtr(pointer), GPRInfo::regT3);
    jit.farJump(GPRInfo::regT3, OperationPtrTag);

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::LLIntThunk);
    return FINALIZE_THUNK(patchBuffer, NativeToJITGatePtrTag, "untag"_s, "untag thunk");
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
        codeRef.construct(FINALIZE_CODE(patchBuffer, NativeToJITGatePtrTag, "jitCagePtr"_s, "jitCagePtr thunk"));
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
    FOR_EACH_LLINT_OPCODE_WITH_RETURN(LLINT_RETURN_LOCATION)
    default:
        RELEASE_ASSERT_NOT_REACHED();
        return { };
    }
}

#endif

} // namespace LLInt

#else // ENABLE(JIT)

namespace LLInt {

#if ENABLE(WEBASSEMBLY)
MacroAssemblerCodeRef<JITThunkPtrTag> wasmFunctionEntryThunk()
{
    return { };
}

MacroAssemblerCodeRef<JITThunkPtrTag> wasmFunctionEntryThunkSIMD()
{
    return { };
}
#endif // ENABLE(WEBASSEMBLY)

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
    return reinterpret_cast<VMEntryRecord*>(reinterpret_cast<intptr_t>(entryFrame) - VMEntryTotalFrameSize);
}

#endif // ENABLE(C_LOOP)

} // namespace JSC
