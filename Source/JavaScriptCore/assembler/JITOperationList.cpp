/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "JITOperationList.h"

#include "Gate.h"
#include "LLIntData.h"
#include "Opcode.h"

#if ENABLE(JIT_CAGE)
#include <WebKitAdditions/JITCageAdditions.h>
#endif

namespace JSC {

LazyNeverDestroyed<JITOperationList> jitOperationList;

#if ENABLE(JIT_OPERATION_VALIDATION)
extern const uintptr_t startOfJITOperationsInJSC __asm("section$start$__DATA_CONST$__jsc_ops");
extern const uintptr_t endOfJITOperationsInJSC __asm("section$end$__DATA_CONST$__jsc_ops");
#endif

void JITOperationList::initialize()
{
    jitOperationList.construct();
}

#if ENABLE(JIT_OPERATION_VALIDATION)
static SUPPRESS_ASAN ALWAYS_INLINE void addPointers(HashMap<void*, void*>& map, const uintptr_t* beginOperations, const uintptr_t* endOperations)
{
#if ENABLE(JIT_CAGE)
    if (Options::useJITCage()) {
        JSC_JIT_CAGED_POINTER_REGISTRATION();
        return;
    }
#endif
    if constexpr (ASSERT_ENABLED) {
        for (const uintptr_t* current = beginOperations; current != endOperations; ++current) {
            void* codePtr = removeCodePtrTag(bitwise_cast<void*>(*current));
            if (codePtr)
                map.add(codePtr, WTF::tagNativeCodePtrImpl<OperationPtrTag>(codePtr));
        }
    }
}
#endif

void JITOperationList::populatePointersInJavaScriptCore()
{
#if ENABLE(JIT_OPERATION_VALIDATION)
    static std::once_flag onceKey;
    std::call_once(onceKey, [] {
        if (Options::useJIT())
            addPointers(jitOperationList->m_validatedOperations, &startOfJITOperationsInJSC, &endOfJITOperationsInJSC);
    });
#endif
}

void JITOperationList::populatePointersInJavaScriptCoreForLLInt()
{
#if ENABLE(JIT_OPERATION_VALIDATION)
    static std::once_flag onceKey;
    std::call_once(onceKey, [] {

#define LLINT_OP(name) \
    bitwise_cast<uintptr_t>(LLInt::getCodeFunctionPtr<CFunctionPtrTag>(name)), \
    bitwise_cast<uintptr_t>(LLInt::getWide16CodeFunctionPtr<CFunctionPtrTag>(name)), \
    bitwise_cast<uintptr_t>(LLInt::getWide32CodeFunctionPtr<CFunctionPtrTag>(name)),

#define LLINT_RETURN_LOCATION(name, ...) \
    LLINT_OP(name##_return_location)

        const uintptr_t operations[] = {
            bitwise_cast<uintptr_t>(LLInt::getCodeFunctionPtr<CFunctionPtrTag>(llint_function_for_call_prologue)),
            bitwise_cast<uintptr_t>(LLInt::getCodeFunctionPtr<CFunctionPtrTag>(llint_function_for_construct_prologue)),
            bitwise_cast<uintptr_t>(LLInt::getCodeFunctionPtr<CFunctionPtrTag>(llint_function_for_call_arity_check)),
            bitwise_cast<uintptr_t>(LLInt::getCodeFunctionPtr<CFunctionPtrTag>(llint_function_for_construct_arity_check)),
            bitwise_cast<uintptr_t>(LLInt::getCodeFunctionPtr<CFunctionPtrTag>(llint_eval_prologue)),
            bitwise_cast<uintptr_t>(LLInt::getCodeFunctionPtr<CFunctionPtrTag>(llint_program_prologue)),
            bitwise_cast<uintptr_t>(LLInt::getCodeFunctionPtr<CFunctionPtrTag>(llint_module_program_prologue)),
            bitwise_cast<uintptr_t>(LLInt::getCodeFunctionPtr<CFunctionPtrTag>(wasm_function_prologue)),
            bitwise_cast<uintptr_t>(LLInt::getCodeFunctionPtr<CFunctionPtrTag>(wasm_function_prologue_no_tls)),
            bitwise_cast<uintptr_t>(LLInt::getCodeFunctionPtr<CFunctionPtrTag>(llint_throw_during_call_trampoline)),
            bitwise_cast<uintptr_t>(LLInt::getCodeFunctionPtr<CFunctionPtrTag>(llint_handle_uncaught_exception)),
            bitwise_cast<uintptr_t>(LLInt::getCodeFunctionPtr<CFunctionPtrTag>(checkpoint_osr_exit_trampoline)),
            bitwise_cast<uintptr_t>(LLInt::getCodeFunctionPtr<CFunctionPtrTag>(checkpoint_osr_exit_from_inlined_call_trampoline)),
            bitwise_cast<uintptr_t>(LLInt::getCodeFunctionPtr<CFunctionPtrTag>(normal_osr_exit_trampoline)),
            bitwise_cast<uintptr_t>(LLInt::getCodeFunctionPtr<CFunctionPtrTag>(fuzzer_return_early_from_loop_hint)),

            LLINT_OP(op_catch)
            LLINT_OP(llint_generic_return_point)

            LLINT_RETURN_LOCATION(op_get_by_id)
            LLINT_RETURN_LOCATION(op_get_by_val)
            LLINT_RETURN_LOCATION(op_put_by_id)
            LLINT_RETURN_LOCATION(op_put_by_val)

            JSC_JS_GATE_OPCODES(LLINT_RETURN_LOCATION)
            JSC_WASM_GATE_OPCODES(LLINT_RETURN_LOCATION)
        };
        if (Options::useJIT())
            addPointers(jitOperationList->m_validatedOperations, operations, operations + WTF_ARRAY_LENGTH(operations));
#undef LLINT_RETURN_LOCATION
    });
#endif
}


void JITOperationList::populatePointersInEmbedder(const uintptr_t* beginOperations, const uintptr_t* endOperations)
{
    UNUSED_PARAM(beginOperations);
    UNUSED_PARAM(endOperations);
#if ENABLE(JIT_OPERATION_VALIDATION)
    if (Options::useJIT())
        addPointers(jitOperationList->m_validatedOperations, beginOperations, endOperations);
#endif
}

} // namespace JSC
