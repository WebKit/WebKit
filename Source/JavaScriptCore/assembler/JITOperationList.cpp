/*
 * Copyright (C) 2020-2021 Apple Inc. All rights reserved.
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
#include "JITOperationValidation.h"
#include "LLIntData.h"
#include "Opcode.h"

namespace JSC {

#if ENABLE(JIT_OPERATION_VALIDATION)

LazyNeverDestroyed<JITOperationList> jitOperationList;

extern const JITOperationAnnotation startOfJITOperationsInJSC __asm("section$start$__DATA_CONST$__jsc_ops");
extern const JITOperationAnnotation endOfJITOperationsInJSC __asm("section$end$__DATA_CONST$__jsc_ops");

void JITOperationList::initialize()
{
    jitOperationList.construct();
}

#if JIT_OPERATION_VALIDATION_ASSERT_ENABLED
void JITOperationList::addInverseMap(void* validationEntry, void* pointer)
{
    m_validatedOperationsInverseMap.add(validationEntry, pointer);
}

#define JSC_REGISTER_INVERSE_JIT_CAGED_POINTER_FOR_DEBUG(validationEntry, pointer) \
    addInverseMap(validationEntry, pointer)
#else
#define JSC_REGISTER_INVERSE_JIT_CAGED_POINTER_FOR_DEBUG(validationEntry, pointer)
#endif // JIT_OPERATION_VALIDATION_ASSERT_ENABLED

SUPPRESS_ASAN ALWAYS_INLINE void JITOperationList::addPointers(const JITOperationAnnotation* begin, const JITOperationAnnotation* end)
{
    auto& map = m_validatedOperations;
#if ENABLE(JIT_CAGE)
    if (Options::useJITCage()) {
        JSC_JIT_CAGED_POINTER_REGISTRATION();
        return;
    }
#endif
    if constexpr (ASSERT_ENABLED) {
        for (const auto* current = begin; current != end; ++current) {
            void* operation = removeCodePtrTag(current->operation);
            if (operation) {
                void* validator = removeCodePtrTag(current->operationWithValidation);
                validator = WTF::tagNativeCodePtrImpl<OperationPtrTag>(validator);
                map.add(operation, validator);
                JSC_REGISTER_INVERSE_JIT_CAGED_POINTER_FOR_DEBUG(validator, operation);
            }
        }
    }
}

void JITOperationList::populatePointersInJavaScriptCore()
{
    static std::once_flag onceKey;
    std::call_once(onceKey, [] {
        if (Options::useJIT())
            jitOperationList->addPointers(&startOfJITOperationsInJSC, &endOfJITOperationsInJSC);
    });
}

LLINT_DECLARE_ROUTINE_VALIDATE(llint_function_for_call_prologue);
LLINT_DECLARE_ROUTINE_VALIDATE(llint_function_for_construct_prologue);
LLINT_DECLARE_ROUTINE_VALIDATE(llint_function_for_call_arity_check);
LLINT_DECLARE_ROUTINE_VALIDATE(llint_function_for_construct_arity_check);
LLINT_DECLARE_ROUTINE_VALIDATE(llint_eval_prologue);
LLINT_DECLARE_ROUTINE_VALIDATE(llint_program_prologue);
LLINT_DECLARE_ROUTINE_VALIDATE(llint_module_program_prologue);
LLINT_DECLARE_ROUTINE_VALIDATE(wasm_function_prologue);
LLINT_DECLARE_ROUTINE_VALIDATE(wasm_function_prologue_no_tls);
LLINT_DECLARE_ROUTINE_VALIDATE(llint_throw_during_call_trampoline);
LLINT_DECLARE_ROUTINE_VALIDATE(llint_handle_uncaught_exception);
LLINT_DECLARE_ROUTINE_VALIDATE(checkpoint_osr_exit_trampoline);
LLINT_DECLARE_ROUTINE_VALIDATE(checkpoint_osr_exit_from_inlined_call_trampoline);
LLINT_DECLARE_ROUTINE_VALIDATE(normal_osr_exit_trampoline);
LLINT_DECLARE_ROUTINE_VALIDATE(fuzzer_return_early_from_loop_hint);

void JITOperationList::populatePointersInJavaScriptCoreForLLInt()
{
    static std::once_flag onceKey;
    std::call_once(onceKey, [] {

#define LLINT_ROUTINE(functionName) { \
        bitwise_cast<void*>(LLInt::getCodeFunctionPtr<CFunctionPtrTag>(functionName)), \
        bitwise_cast<void*>(LLINT_ROUTINE_VALIDATE(functionName)) \
    },

#define LLINT_OP(name) { \
        bitwise_cast<void*>(LLInt::getCodeFunctionPtr<CFunctionPtrTag>(name)), \
        bitwise_cast<void*>(LLINT_RETURN_VALIDATE(name)) \
    }, { \
        bitwise_cast<void*>(LLInt::getWide16CodeFunctionPtr<CFunctionPtrTag>(name)), \
        bitwise_cast<void*>(LLINT_RETURN_WIDE16_VALIDATE(name)) \
    }, { \
        bitwise_cast<void*>(LLInt::getWide32CodeFunctionPtr<CFunctionPtrTag>(name)), \
        bitwise_cast<void*>(LLINT_RETURN_WIDE32_VALIDATE(name)) \
    },

#define LLINT_RETURN_LOCATION(name, ...) \
    LLINT_OP(name##_return_location)

        const JITOperationAnnotation operations[] = {
            LLINT_ROUTINE(llint_function_for_call_prologue)
            LLINT_ROUTINE(llint_function_for_construct_prologue)
            LLINT_ROUTINE(llint_function_for_call_arity_check)
            LLINT_ROUTINE(llint_function_for_construct_arity_check)
            LLINT_ROUTINE(llint_eval_prologue)
            LLINT_ROUTINE(llint_program_prologue)
            LLINT_ROUTINE(llint_module_program_prologue)
            LLINT_ROUTINE(wasm_function_prologue)
            LLINT_ROUTINE(wasm_function_prologue_no_tls)
            LLINT_ROUTINE(llint_throw_during_call_trampoline)
            LLINT_ROUTINE(llint_handle_uncaught_exception)
            LLINT_ROUTINE(checkpoint_osr_exit_trampoline)
            LLINT_ROUTINE(checkpoint_osr_exit_from_inlined_call_trampoline)
            LLINT_ROUTINE(normal_osr_exit_trampoline)
            LLINT_ROUTINE(fuzzer_return_early_from_loop_hint)

            LLINT_OP(op_catch)
            LLINT_OP(wasm_catch)
            LLINT_OP(wasm_catch_no_tls)
            LLINT_OP(wasm_catch_all)
            LLINT_OP(wasm_catch_all_no_tls)
            LLINT_OP(llint_generic_return_point)

            LLINT_RETURN_LOCATION(op_get_by_id)
            LLINT_RETURN_LOCATION(op_get_by_val)
            LLINT_RETURN_LOCATION(op_put_by_id)
            LLINT_RETURN_LOCATION(op_put_by_val)

            JSC_JS_GATE_OPCODES(LLINT_RETURN_LOCATION)
            JSC_WASM_GATE_OPCODES(LLINT_RETURN_LOCATION)
        };
        if (Options::useJIT())
            jitOperationList->addPointers(operations, operations + WTF_ARRAY_LENGTH(operations));
#undef LLINT_ROUTINE
#undef LLINT_OP
#undef LLINT_RETURN_LOCATION
    });
}


void JITOperationList::populatePointersInEmbedder(const JITOperationAnnotation* beginOperations, const JITOperationAnnotation* endOperations)
{
    if (Options::useJIT())
        jitOperationList->addPointers(beginOperations, endOperations);
}

#endif // ENABLE(JIT_OPERATION_VALIDATION)

} // namespace JSC
