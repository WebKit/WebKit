/*
 * Copyright (C) 2011-2021 Apple Inc. All rights reserved.
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
#include "JSPropertyNameEnumerator.h"
#include "ScopedArguments.h"
#include "SlowPathFunction.h"
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
    if (static_cast<unsigned>(argumentCountIncludingThis) >= codeBlock->numParameters())
        return 0;
    int alignedFrameSize = WTF::roundUpToMultipleOf(stackAlignmentRegisters(), argumentCountIncludingThis + CallFrame::headerSizeInRegisters);
    int alignedFrameSizeForParameters = WTF::roundUpToMultipleOf(stackAlignmentRegisters(), codeBlock->numParameters() + CallFrame::headerSizeInRegisters);
    return alignedFrameSizeForParameters - alignedFrameSize;
}

ALWAYS_INLINE int numberOfStackPaddingSlotsWithExtraSlots(CodeBlock* codeBlock, int argumentCountIncludingThis)
{
    if (static_cast<unsigned>(argumentCountIncludingThis) >= codeBlock->numParameters())
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

inline JSValue opEnumeratorGetByVal(JSGlobalObject* globalObject, JSValue baseValue, JSValue propertyNameValue, unsigned index, JSPropertyNameEnumerator::Mode mode, JSPropertyNameEnumerator* enumerator, ArrayProfile* arrayProfile = nullptr, uint8_t* enumeratorMetadata = nullptr)
{
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);

    switch (mode) {
    case JSPropertyNameEnumerator::IndexedMode: {
        if (arrayProfile && LIKELY(baseValue.isCell()))
            arrayProfile->observeStructureID(baseValue.asCell()->structureID());
        RELEASE_AND_RETURN(scope, baseValue.get(globalObject, static_cast<unsigned>(index)));
    }
    case JSPropertyNameEnumerator::OwnStructureMode: {
        if (LIKELY(baseValue.isCell()) && baseValue.asCell()->structureID() == enumerator->cachedStructureID()) {
            // We'll only match the structure ID if the base is an object.
            ASSERT(index < enumerator->endStructurePropertyIndex());
            RELEASE_AND_RETURN(scope, baseValue.getObject()->getDirect(index < enumerator->cachedInlineCapacity() ? index : index - enumerator->cachedInlineCapacity() + firstOutOfLineOffset));
        } else {
            if (enumeratorMetadata)
                *enumeratorMetadata |= static_cast<uint8_t>(JSPropertyNameEnumerator::HasSeenOwnStructureModeStructureMismatch);
        }
        FALLTHROUGH;
    }

    case JSPropertyNameEnumerator::GenericMode: {
        if (arrayProfile && baseValue.isCell() && mode != JSPropertyNameEnumerator::OwnStructureMode)
            arrayProfile->observeStructureID(baseValue.asCell()->structureID());
#if USE(JSVALUE32_64)
        if (!propertyNameValue.isCell()) {
            // This branch is only needed because we use this method
            // both as a slow_path and as a DFG call op. We'll end up
            // here if propertyName is not a cell then we are in
            // index+named mode, so do what RecoverNameAndGetVal
            // does. This can probably be removed if we re-enable the
            // optimizations for enumeratorGetByVal in DFG, see bug
            // #230189.
            JSString* string = enumerator->propertyNameAtIndex(index);
            auto propertyName = string->toIdentifier(globalObject);
            RETURN_IF_EXCEPTION(scope, { });
            RELEASE_AND_RETURN(scope, baseValue.get(globalObject, propertyName));
        }
#endif
        JSString* string = asString(propertyNameValue);
        auto propertyName = string->toIdentifier(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
        RELEASE_AND_RETURN(scope, baseValue.get(globalObject, propertyName));
    }

    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    };
    RELEASE_ASSERT_NOT_REACHED();
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

#define JSC_DECLARE_COMMON_SLOW_PATH(name) \
    extern "C" JSC_DECLARE_JIT_OPERATION(name, SlowPathReturnType, (CallFrame*, const Instruction*))

#define JSC_DEFINE_COMMON_SLOW_PATH(name) \
    JSC_DEFINE_JIT_OPERATION(name, SlowPathReturnType, (CallFrame* callFrame, const Instruction* pc))

JSC_DECLARE_COMMON_SLOW_PATH(slow_path_call_arityCheck);
JSC_DECLARE_COMMON_SLOW_PATH(slow_path_construct_arityCheck);
JSC_DECLARE_COMMON_SLOW_PATH(slow_path_create_direct_arguments);
JSC_DECLARE_COMMON_SLOW_PATH(slow_path_create_scoped_arguments);
JSC_DECLARE_COMMON_SLOW_PATH(slow_path_create_cloned_arguments);
JSC_DECLARE_COMMON_SLOW_PATH(slow_path_create_arguments_butterfly);
JSC_DECLARE_COMMON_SLOW_PATH(slow_path_create_this);
JSC_DECLARE_COMMON_SLOW_PATH(slow_path_enter);
JSC_DECLARE_COMMON_SLOW_PATH(slow_path_to_this);
JSC_DECLARE_COMMON_SLOW_PATH(slow_path_check_tdz);
JSC_DECLARE_COMMON_SLOW_PATH(slow_path_throw_strict_mode_readonly_property_write_error);
JSC_DECLARE_COMMON_SLOW_PATH(slow_path_not);
JSC_DECLARE_COMMON_SLOW_PATH(slow_path_eq);
JSC_DECLARE_COMMON_SLOW_PATH(slow_path_neq);
JSC_DECLARE_COMMON_SLOW_PATH(slow_path_stricteq);
JSC_DECLARE_COMMON_SLOW_PATH(slow_path_nstricteq);
JSC_DECLARE_COMMON_SLOW_PATH(slow_path_less);
JSC_DECLARE_COMMON_SLOW_PATH(slow_path_lesseq);
JSC_DECLARE_COMMON_SLOW_PATH(slow_path_greater);
JSC_DECLARE_COMMON_SLOW_PATH(slow_path_greatereq);
JSC_DECLARE_COMMON_SLOW_PATH(slow_path_inc);
JSC_DECLARE_COMMON_SLOW_PATH(slow_path_dec);
JSC_DECLARE_COMMON_SLOW_PATH(slow_path_to_number);
JSC_DECLARE_COMMON_SLOW_PATH(slow_path_to_numeric);
JSC_DECLARE_COMMON_SLOW_PATH(slow_path_to_string);
JSC_DECLARE_COMMON_SLOW_PATH(slow_path_to_object);
JSC_DECLARE_COMMON_SLOW_PATH(slow_path_negate);
JSC_DECLARE_COMMON_SLOW_PATH(slow_path_add);
JSC_DECLARE_COMMON_SLOW_PATH(slow_path_mul);
JSC_DECLARE_COMMON_SLOW_PATH(slow_path_sub);
JSC_DECLARE_COMMON_SLOW_PATH(slow_path_div);
JSC_DECLARE_COMMON_SLOW_PATH(slow_path_mod);
JSC_DECLARE_COMMON_SLOW_PATH(slow_path_pow);
JSC_DECLARE_COMMON_SLOW_PATH(slow_path_lshift);
JSC_DECLARE_COMMON_SLOW_PATH(slow_path_rshift);
JSC_DECLARE_COMMON_SLOW_PATH(slow_path_urshift);
JSC_DECLARE_COMMON_SLOW_PATH(slow_path_unsigned);
JSC_DECLARE_COMMON_SLOW_PATH(slow_path_bitnot);
JSC_DECLARE_COMMON_SLOW_PATH(slow_path_bitand);
JSC_DECLARE_COMMON_SLOW_PATH(slow_path_bitor);
JSC_DECLARE_COMMON_SLOW_PATH(slow_path_bitxor);
JSC_DECLARE_COMMON_SLOW_PATH(slow_path_typeof);
JSC_DECLARE_COMMON_SLOW_PATH(slow_path_typeof_is_object);
JSC_DECLARE_COMMON_SLOW_PATH(slow_path_typeof_is_function);
JSC_DECLARE_COMMON_SLOW_PATH(slow_path_is_callable);
JSC_DECLARE_COMMON_SLOW_PATH(slow_path_is_constructor);
JSC_DECLARE_COMMON_SLOW_PATH(slow_path_strcat);
JSC_DECLARE_COMMON_SLOW_PATH(slow_path_to_primitive);
JSC_DECLARE_COMMON_SLOW_PATH(slow_path_to_property_key);
JSC_DECLARE_COMMON_SLOW_PATH(slow_path_get_property_enumerator);
JSC_DECLARE_COMMON_SLOW_PATH(slow_path_enumerator_next);
JSC_DECLARE_COMMON_SLOW_PATH(slow_path_enumerator_get_by_val);
JSC_DECLARE_COMMON_SLOW_PATH(slow_path_enumerator_in_by_val);
JSC_DECLARE_COMMON_SLOW_PATH(slow_path_enumerator_has_own_property);
JSC_DECLARE_COMMON_SLOW_PATH(slow_path_profile_type_clear_log);
JSC_DECLARE_COMMON_SLOW_PATH(slow_path_unreachable);
JSC_DECLARE_COMMON_SLOW_PATH(slow_path_create_lexical_environment);
JSC_DECLARE_COMMON_SLOW_PATH(slow_path_push_with_scope);
JSC_DECLARE_COMMON_SLOW_PATH(slow_path_resolve_scope);
JSC_DECLARE_COMMON_SLOW_PATH(slow_path_resolve_scope_for_hoisting_func_decl_in_eval);
JSC_DECLARE_COMMON_SLOW_PATH(slow_path_create_promise);
JSC_DECLARE_COMMON_SLOW_PATH(slow_path_create_generator);
JSC_DECLARE_COMMON_SLOW_PATH(slow_path_create_async_generator);
JSC_DECLARE_COMMON_SLOW_PATH(slow_path_create_rest);
JSC_DECLARE_COMMON_SLOW_PATH(slow_path_get_by_val_with_this);
JSC_DECLARE_COMMON_SLOW_PATH(slow_path_get_prototype_of);
JSC_DECLARE_COMMON_SLOW_PATH(slow_path_put_by_id_with_this);
JSC_DECLARE_COMMON_SLOW_PATH(slow_path_put_by_val_with_this);
JSC_DECLARE_COMMON_SLOW_PATH(slow_path_define_data_property);
JSC_DECLARE_COMMON_SLOW_PATH(slow_path_define_accessor_property);
JSC_DECLARE_COMMON_SLOW_PATH(slow_path_throw_static_error);
JSC_DECLARE_COMMON_SLOW_PATH(slow_path_new_promise);
JSC_DECLARE_COMMON_SLOW_PATH(slow_path_new_generator);
JSC_DECLARE_COMMON_SLOW_PATH(slow_path_new_array_with_spread);
JSC_DECLARE_COMMON_SLOW_PATH(slow_path_new_array_buffer);
JSC_DECLARE_COMMON_SLOW_PATH(slow_path_spread);
JSC_DECLARE_COMMON_SLOW_PATH(iterator_open_try_fast_narrow);
JSC_DECLARE_COMMON_SLOW_PATH(iterator_open_try_fast_wide16);
JSC_DECLARE_COMMON_SLOW_PATH(iterator_open_try_fast_wide32);
JSC_DECLARE_COMMON_SLOW_PATH(iterator_next_try_fast_narrow);
JSC_DECLARE_COMMON_SLOW_PATH(iterator_next_try_fast_wide16);
JSC_DECLARE_COMMON_SLOW_PATH(iterator_next_try_fast_wide32);

} // namespace JSC
