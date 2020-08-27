/*
 * Copyright (C) 2011-2019 Apple Inc. All rights reserved.
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

#include "CodeBlock.h"
#include "CodeSpecializationKind.h"
#include "DirectArguments.h"
#include "ExceptionHelpers.h"
#include "FunctionCodeBlock.h"
#include "JSImmutableButterfly.h"
#include "ScopedArguments.h"
#include "SlowPathReturnType.h"
#include "StackAlignment.h"
#include "VMInlines.h"
#include <wtf/StdLibExtras.h>

namespace JSC {

// The purpose of this namespace is to include slow paths that are shared
// between the interpreter and baseline JIT. They are written to be agnostic
// with respect to the slow-path calling convention, but they do rely on the
// JS code being executed more-or-less directly from bytecode (so the call
// frame layout is unmodified, making it potentially awkward to use these
// from any optimizing JIT, like the DFG).

namespace CommonSlowPaths {

ALWAYS_INLINE int numberOfExtraSlots(int argumentCountIncludingThis)
{
    int frameSize = argumentCountIncludingThis + CallFrame::headerSizeInRegisters;
    int alignedFrameSize = WTF::roundUpToMultipleOf(stackAlignmentRegisters(), frameSize);
    return alignedFrameSize - frameSize;
}

ALWAYS_INLINE int numberOfStackPaddingSlots(CodeBlock* codeBlock, int argumentCountIncludingThis)
{
    if (argumentCountIncludingThis >= codeBlock->numParameters())
        return 0;
    int alignedFrameSize = WTF::roundUpToMultipleOf(stackAlignmentRegisters(), argumentCountIncludingThis + CallFrame::headerSizeInRegisters);
    int alignedFrameSizeForParameters = WTF::roundUpToMultipleOf(stackAlignmentRegisters(), codeBlock->numParameters() + CallFrame::headerSizeInRegisters);
    return alignedFrameSizeForParameters - alignedFrameSize;
}

ALWAYS_INLINE int numberOfStackPaddingSlotsWithExtraSlots(CodeBlock* codeBlock, int argumentCountIncludingThis)
{
    if (argumentCountIncludingThis >= codeBlock->numParameters())
        return 0;
    return numberOfStackPaddingSlots(codeBlock, argumentCountIncludingThis) + numberOfExtraSlots(argumentCountIncludingThis);
}

ALWAYS_INLINE CodeBlock* codeBlockFromCallFrameCallee(CallFrame* callFrame, CodeSpecializationKind kind)
{
    JSFunction* callee = jsCast<JSFunction*>(callFrame->jsCallee());
    ASSERT(!callee->isHostFunction());
    return callee->jsExecutable()->codeBlockFor(kind);
}

ALWAYS_INLINE int arityCheckFor(VM& vm, CallFrame* callFrame, CodeSpecializationKind kind)
{
    CodeBlock* newCodeBlock = codeBlockFromCallFrameCallee(callFrame, kind);
    ASSERT(callFrame->argumentCountIncludingThis() < static_cast<unsigned>(newCodeBlock->numParameters()));
    int padding = numberOfStackPaddingSlotsWithExtraSlots(newCodeBlock, callFrame->argumentCountIncludingThis());
    
    Register* newStack = callFrame->registers() - WTF::roundUpToMultipleOf(stackAlignmentRegisters(), padding);

    if (UNLIKELY(!vm.ensureStackCapacityFor(newStack)))
        return -1;
    return padding;
}

inline bool opInByVal(JSGlobalObject* globalObject, JSValue baseVal, JSValue propName, ArrayProfile* arrayProfile = nullptr)
{
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);
    if (!baseVal.isObject()) {
        throwException(globalObject, scope, createInvalidInParameterError(globalObject, baseVal));
        return false;
    }

    JSObject* baseObj = asObject(baseVal);
    if (arrayProfile)
        arrayProfile->observeStructure(baseObj->structure(vm));

    uint32_t i;
    if (propName.getUInt32(i)) {
        if (arrayProfile)
            arrayProfile->observeIndexedRead(vm, baseObj, i);
        RELEASE_AND_RETURN(scope, baseObj->hasProperty(globalObject, i));
    }

    auto property = propName.toPropertyKey(globalObject);
    RETURN_IF_EXCEPTION(scope, false);
    RELEASE_AND_RETURN(scope, baseObj->hasProperty(globalObject, property));
}

inline bool canAccessArgumentIndexQuickly(JSObject& object, uint32_t index)
{
    switch (object.type()) {
    case DirectArgumentsType: {
        DirectArguments* directArguments = jsCast<DirectArguments*>(&object);
        if (directArguments->isMappedArgumentInDFG(index))
            return true;
        break;
    }
    case ScopedArgumentsType: {
        ScopedArguments* scopedArguments = jsCast<ScopedArguments*>(&object);
        if (scopedArguments->isMappedArgumentInDFG(index))
            return true;
        break;
    }
    default:
        break;
    }
    return false;
}

ALWAYS_INLINE Structure* originalStructureBeforePut(VM& vm, JSValue value)
{
    if (!value.isCell())
        return nullptr;
    if (value.asCell()->type() == PureForwardingProxyType)
        return jsCast<JSProxy*>(value)->target()->structure(vm);
    return value.asCell()->structure(vm);
}


static ALWAYS_INLINE void putDirectWithReify(VM& vm, JSGlobalObject* globalObject, JSObject* baseObject, PropertyName propertyName, JSValue value, PutPropertySlot& slot, Structure** result = nullptr)
{
    auto scope = DECLARE_THROW_SCOPE(vm);
    if (baseObject->inherits<JSFunction>(vm)) {
        jsCast<JSFunction*>(baseObject)->reifyLazyPropertyIfNeeded(vm, globalObject, propertyName);
        RETURN_IF_EXCEPTION(scope, void());
    }
    if (result)
        *result = originalStructureBeforePut(vm, baseObject);
    scope.release();
    baseObject->putDirect(vm, propertyName, value, slot);
}

static ALWAYS_INLINE void putDirectAccessorWithReify(VM& vm, JSGlobalObject* globalObject, JSObject* baseObject, PropertyName propertyName, GetterSetter* accessor, unsigned attribute)
{
    auto scope = DECLARE_THROW_SCOPE(vm);
    if (baseObject->inherits<JSFunction>(vm)) {
        jsCast<JSFunction*>(baseObject)->reifyLazyPropertyIfNeeded(vm, globalObject, propertyName);
        RETURN_IF_EXCEPTION(scope, void());
    }
    scope.release();
    baseObject->putDirectAccessor(globalObject, propertyName, accessor, attribute);
}

inline JSArray* allocateNewArrayBuffer(VM& vm, Structure* structure, JSImmutableButterfly* immutableButterfly)
{
    JSGlobalObject* globalObject = structure->globalObject();
    Structure* originalStructure = globalObject->originalArrayStructureForIndexingType(immutableButterfly->indexingMode());
    ASSERT(originalStructure->indexingMode() == immutableButterfly->indexingMode());
    ASSERT(isCopyOnWrite(immutableButterfly->indexingMode()));
    ASSERT(!structure->outOfLineCapacity());

    JSArray* result = JSArray::createWithButterfly(vm, nullptr, originalStructure, immutableButterfly->toButterfly());
    // FIXME: This works but it's slow. If we cared enough about the perf when having a bad time then we could fix it.
    if (UNLIKELY(originalStructure != structure)) {
        ASSERT(hasSlowPutArrayStorage(structure->indexingMode()));
        ASSERT(globalObject->isHavingABadTime());

        result->switchToSlowPutArrayStorage(vm);
        ASSERT(result->butterfly() != immutableButterfly->toButterfly());
        ASSERT(!result->butterfly()->arrayStorage()->m_sparseMap.get());
        ASSERT(result->structureID() == structure->id());
    }

    return result;
}

} // namespace CommonSlowPaths

class CallFrame;
struct Instruction;

#define SLOW_PATH
    
#define SLOW_PATH_DECL(name) \
extern "C" SlowPathReturnType SLOW_PATH name(CallFrame* callFrame, const Instruction* pc)
    
#define SLOW_PATH_HIDDEN_DECL(name) \
SLOW_PATH_DECL(name) WTF_INTERNAL
    
SLOW_PATH_HIDDEN_DECL(slow_path_call_arityCheck);
SLOW_PATH_HIDDEN_DECL(slow_path_construct_arityCheck);
SLOW_PATH_HIDDEN_DECL(slow_path_create_direct_arguments);
SLOW_PATH_HIDDEN_DECL(slow_path_create_scoped_arguments);
SLOW_PATH_HIDDEN_DECL(slow_path_create_cloned_arguments);
SLOW_PATH_HIDDEN_DECL(slow_path_create_arguments_butterfly);
SLOW_PATH_HIDDEN_DECL(slow_path_create_this);
SLOW_PATH_HIDDEN_DECL(slow_path_enter);
SLOW_PATH_HIDDEN_DECL(slow_path_get_callee);
SLOW_PATH_HIDDEN_DECL(slow_path_to_this);
SLOW_PATH_HIDDEN_DECL(slow_path_throw_tdz_error);
SLOW_PATH_HIDDEN_DECL(slow_path_check_tdz);
SLOW_PATH_HIDDEN_DECL(slow_path_throw_strict_mode_readonly_property_write_error);
SLOW_PATH_HIDDEN_DECL(slow_path_not);
SLOW_PATH_HIDDEN_DECL(slow_path_eq);
SLOW_PATH_HIDDEN_DECL(slow_path_neq);
SLOW_PATH_HIDDEN_DECL(slow_path_stricteq);
SLOW_PATH_HIDDEN_DECL(slow_path_nstricteq);
SLOW_PATH_HIDDEN_DECL(slow_path_less);
SLOW_PATH_HIDDEN_DECL(slow_path_lesseq);
SLOW_PATH_HIDDEN_DECL(slow_path_greater);
SLOW_PATH_HIDDEN_DECL(slow_path_greatereq);
SLOW_PATH_HIDDEN_DECL(slow_path_inc);
SLOW_PATH_HIDDEN_DECL(slow_path_dec);
SLOW_PATH_HIDDEN_DECL(slow_path_to_number);
SLOW_PATH_HIDDEN_DECL(slow_path_to_numeric);
SLOW_PATH_HIDDEN_DECL(slow_path_to_string);
SLOW_PATH_HIDDEN_DECL(slow_path_to_object);
SLOW_PATH_HIDDEN_DECL(slow_path_negate);
SLOW_PATH_HIDDEN_DECL(slow_path_add);
SLOW_PATH_HIDDEN_DECL(slow_path_mul);
SLOW_PATH_HIDDEN_DECL(slow_path_sub);
SLOW_PATH_HIDDEN_DECL(slow_path_div);
SLOW_PATH_HIDDEN_DECL(slow_path_mod);
SLOW_PATH_HIDDEN_DECL(slow_path_pow);
SLOW_PATH_HIDDEN_DECL(slow_path_lshift);
SLOW_PATH_HIDDEN_DECL(slow_path_rshift);
SLOW_PATH_HIDDEN_DECL(slow_path_urshift);
SLOW_PATH_HIDDEN_DECL(slow_path_unsigned);
SLOW_PATH_HIDDEN_DECL(slow_path_bitnot);
SLOW_PATH_HIDDEN_DECL(slow_path_bitand);
SLOW_PATH_HIDDEN_DECL(slow_path_bitor);
SLOW_PATH_HIDDEN_DECL(slow_path_bitxor);
SLOW_PATH_HIDDEN_DECL(slow_path_typeof);
SLOW_PATH_HIDDEN_DECL(slow_path_typeof_is_object);
SLOW_PATH_HIDDEN_DECL(slow_path_typeof_is_function);
SLOW_PATH_HIDDEN_DECL(slow_path_is_callable);
SLOW_PATH_HIDDEN_DECL(slow_path_is_constructor);
SLOW_PATH_HIDDEN_DECL(slow_path_in_by_id);
SLOW_PATH_HIDDEN_DECL(slow_path_in_by_val);
SLOW_PATH_HIDDEN_DECL(slow_path_del_by_val);
SLOW_PATH_HIDDEN_DECL(slow_path_strcat);
SLOW_PATH_HIDDEN_DECL(slow_path_to_primitive);
SLOW_PATH_HIDDEN_DECL(slow_path_to_property_key);
SLOW_PATH_HIDDEN_DECL(slow_path_get_enumerable_length);
SLOW_PATH_HIDDEN_DECL(slow_path_has_generic_property);
SLOW_PATH_HIDDEN_DECL(slow_path_has_structure_property);
SLOW_PATH_HIDDEN_DECL(slow_path_has_own_structure_property);
SLOW_PATH_HIDDEN_DECL(slow_path_in_structure_property);
SLOW_PATH_HIDDEN_DECL(slow_path_has_indexed_property);
SLOW_PATH_HIDDEN_DECL(slow_path_get_direct_pname);
SLOW_PATH_HIDDEN_DECL(slow_path_get_property_enumerator);
SLOW_PATH_HIDDEN_DECL(slow_path_enumerator_structure_pname);
SLOW_PATH_HIDDEN_DECL(slow_path_enumerator_generic_pname);
SLOW_PATH_HIDDEN_DECL(slow_path_to_index_string);
SLOW_PATH_HIDDEN_DECL(slow_path_profile_type_clear_log);
SLOW_PATH_HIDDEN_DECL(slow_path_unreachable);
SLOW_PATH_HIDDEN_DECL(slow_path_create_lexical_environment);
SLOW_PATH_HIDDEN_DECL(slow_path_push_with_scope);
SLOW_PATH_HIDDEN_DECL(slow_path_resolve_scope);
SLOW_PATH_HIDDEN_DECL(slow_path_is_var_scope);
SLOW_PATH_HIDDEN_DECL(slow_path_resolve_scope_for_hoisting_func_decl_in_eval);
SLOW_PATH_HIDDEN_DECL(slow_path_create_promise);
SLOW_PATH_HIDDEN_DECL(slow_path_create_generator);
SLOW_PATH_HIDDEN_DECL(slow_path_create_async_generator);
SLOW_PATH_HIDDEN_DECL(slow_path_create_rest);
SLOW_PATH_HIDDEN_DECL(slow_path_get_by_id_with_this);
SLOW_PATH_HIDDEN_DECL(slow_path_get_by_val_with_this);
SLOW_PATH_HIDDEN_DECL(slow_path_get_private_name);
SLOW_PATH_HIDDEN_DECL(slow_path_get_prototype_of);
SLOW_PATH_HIDDEN_DECL(slow_path_put_by_id_with_this);
SLOW_PATH_HIDDEN_DECL(slow_path_put_by_val_with_this);
SLOW_PATH_HIDDEN_DECL(slow_path_define_data_property);
SLOW_PATH_HIDDEN_DECL(slow_path_define_accessor_property);
SLOW_PATH_HIDDEN_DECL(slow_path_throw_static_error);
SLOW_PATH_HIDDEN_DECL(slow_path_new_promise);
SLOW_PATH_HIDDEN_DECL(slow_path_new_generator);
SLOW_PATH_HIDDEN_DECL(slow_path_new_array_with_spread);
SLOW_PATH_HIDDEN_DECL(slow_path_new_array_buffer);
SLOW_PATH_HIDDEN_DECL(slow_path_spread);
SLOW_PATH_HIDDEN_DECL(iterator_open_try_fast_narrow);
SLOW_PATH_HIDDEN_DECL(iterator_open_try_fast_wide16);
SLOW_PATH_HIDDEN_DECL(iterator_open_try_fast_wide32);
SLOW_PATH_HIDDEN_DECL(iterator_next_try_fast_narrow);
SLOW_PATH_HIDDEN_DECL(iterator_next_try_fast_wide16);
SLOW_PATH_HIDDEN_DECL(iterator_next_try_fast_wide32);

using SlowPathFunction = SlowPathReturnType(SLOW_PATH *)(CallFrame*, const Instruction*);

} // namespace JSC
