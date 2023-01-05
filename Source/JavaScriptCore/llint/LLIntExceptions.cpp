/*
 * Copyright (C) 2011-2020 Apple Inc. All rights reserved.
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
#include "LLIntExceptions.h"

#include "LLIntCommon.h"
#include "LLIntData.h"
#include "LLIntThunks.h"
#include "WasmContextInlines.h"

#if LLINT_TRACING
#include "CatchScope.h"
#include "Exception.h"
#endif

namespace JSC { namespace LLInt {

JSInstruction* returnToThrow(VM& vm)
{
    UNUSED_PARAM(vm);
#if LLINT_TRACING
    if (UNLIKELY(Options::traceLLIntSlowPath())) {
        auto scope = DECLARE_CATCH_SCOPE(vm);
        dataLog("Throwing exception ", JSValue(scope.exception()), " (returnToThrow).\n");
    }
#endif
    return LLInt::exceptionInstructions();
}

WasmInstruction* wasmReturnToThrow(VM& vm)
{
    UNUSED_PARAM(vm);
#if LLINT_TRACING
    if (UNLIKELY(Options::traceLLIntSlowPath())) {
        auto scope = DECLARE_CATCH_SCOPE(vm);
        dataLog("Throwing exception ", JSValue(scope.exception()), " (returnToThrow).\n");
    }
#endif
    return LLInt::wasmExceptionInstructions();
}

MacroAssemblerCodeRef<ExceptionHandlerPtrTag> callToThrow(VM& vm)
{
    UNUSED_PARAM(vm);
#if LLINT_TRACING
    if (UNLIKELY(Options::traceLLIntSlowPath())) {
        auto scope = DECLARE_CATCH_SCOPE(vm);
        dataLog("Throwing exception ", JSValue(scope.exception()), " (callToThrow).\n");
    }
#endif
#if ENABLE(JIT)
    if (Options::useJIT())
        return LLInt::callToThrowThunk();
#endif
    return LLInt::getCodeRef<ExceptionHandlerPtrTag>(llint_throw_during_call_trampoline);
}

MacroAssemblerCodeRef<ExceptionHandlerPtrTag> handleUncaughtException(VM&)
{
#if ENABLE(JIT)
    if (Options::useJIT())
        return handleUncaughtExceptionThunk();
#endif
    return LLInt::getCodeRef<ExceptionHandlerPtrTag>(llint_handle_uncaught_exception);
}

MacroAssemblerCodeRef<ExceptionHandlerPtrTag> handleCatch(OpcodeSize size)
{
#if ENABLE(JIT)
    if (Options::useJIT())
        return handleCatchThunk(size);
#endif
    switch (size) {
    case OpcodeSize::Narrow:
        return LLInt::getCodeRef<ExceptionHandlerPtrTag>(op_catch);
    case OpcodeSize::Wide16:
        return LLInt::getWide16CodeRef<ExceptionHandlerPtrTag>(op_catch);
    case OpcodeSize::Wide32:
        return LLInt::getWide32CodeRef<ExceptionHandlerPtrTag>(op_catch);
    }
    RELEASE_ASSERT_NOT_REACHED();
    return {};
}

#if ENABLE(WEBASSEMBLY)
MacroAssemblerCodeRef<ExceptionHandlerPtrTag> handleWasmCatch(OpcodeSize size)
{
#if ENABLE(JIT)
    if (Options::useJIT())
        return handleWasmCatchThunk(size);
#endif
    WasmOpcodeID opcode = wasm_catch;
    switch (size) {
    case OpcodeSize::Narrow:
        return LLInt::getCodeRef<ExceptionHandlerPtrTag>(opcode);
    case OpcodeSize::Wide16:
        return LLInt::getWide16CodeRef<ExceptionHandlerPtrTag>(opcode);
    case OpcodeSize::Wide32:
        return LLInt::getWide32CodeRef<ExceptionHandlerPtrTag>(opcode);
    }
    RELEASE_ASSERT_NOT_REACHED();
    return { };
}

MacroAssemblerCodeRef<ExceptionHandlerPtrTag> handleWasmCatchAll(OpcodeSize size)
{
#if ENABLE(JIT)
    if (Options::useJIT())
        return handleWasmCatchAllThunk(size);
#endif
    WasmOpcodeID opcode = wasm_catch_all;
    switch (size) {
    case OpcodeSize::Narrow:
        return LLInt::getCodeRef<ExceptionHandlerPtrTag>(opcode);
    case OpcodeSize::Wide16:
        return LLInt::getWide16CodeRef<ExceptionHandlerPtrTag>(opcode);
    case OpcodeSize::Wide32:
        return LLInt::getWide32CodeRef<ExceptionHandlerPtrTag>(opcode);
    }
    RELEASE_ASSERT_NOT_REACHED();
    return { };
}
#endif // ENABLE(WEBASSEMBLY)

} } // namespace JSC::LLInt
