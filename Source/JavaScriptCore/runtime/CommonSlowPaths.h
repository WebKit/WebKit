/*
 * Copyright (C) 2011-2017 Apple Inc. All rights reserved.
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

#include "BytecodeStructs.h"
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

ALWAYS_INLINE int arityCheckFor(ExecState* exec, VM& vm, CodeSpecializationKind kind)
{
    JSFunction* callee = jsCast<JSFunction*>(exec->jsCallee());
    ASSERT(!callee->isHostFunction());
    CodeBlock* newCodeBlock = callee->jsExecutable()->codeBlockFor(kind);
    ASSERT(exec->argumentCountIncludingThis() < static_cast<unsigned>(newCodeBlock->numParameters()));
    int padding = numberOfStackPaddingSlotsWithExtraSlots(newCodeBlock, exec->argumentCountIncludingThis());
    
    Register* newStack = exec->registers() - WTF::roundUpToMultipleOf(stackAlignmentRegisters(), padding);

    if (UNLIKELY(!vm.ensureStackCapacityFor(newStack)))
        return -1;
    return padding;
}

inline bool opInByVal(ExecState* exec, JSValue baseVal, JSValue propName, ArrayProfile* arrayProfile = nullptr)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    if (!baseVal.isObject()) {
        throwException(exec, scope, createInvalidInParameterError(exec, baseVal));
        return false;
    }

    JSObject* baseObj = asObject(baseVal);
    if (arrayProfile)
        arrayProfile->observeStructure(baseObj->structure(vm));

    uint32_t i;
    if (propName.getUInt32(i)) {
        if (arrayProfile)
            arrayProfile->observeIndexedRead(vm, baseObj, i);
        RELEASE_AND_RETURN(scope, baseObj->hasProperty(exec, i));
    }

    auto property = propName.toPropertyKey(exec);
    RETURN_IF_EXCEPTION(scope, false);
    RELEASE_AND_RETURN(scope, baseObj->hasProperty(exec, property));
}

inline void tryCachePutToScopeGlobal(
    ExecState* exec, CodeBlock* codeBlock, OpPutToScope& bytecode, JSObject* scope,
    PutPropertySlot& slot, const Identifier& ident)
{
    // Covers implicit globals. Since they don't exist until they first execute, we didn't know how to cache them at compile time.
    auto& metadata = bytecode.metadata(exec);
    ResolveType resolveType = metadata.getPutInfo.resolveType();
    if (resolveType != GlobalProperty && resolveType != GlobalPropertyWithVarInjectionChecks 
        && resolveType != UnresolvedProperty && resolveType != UnresolvedPropertyWithVarInjectionChecks)
        return;

    if (resolveType == UnresolvedProperty || resolveType == UnresolvedPropertyWithVarInjectionChecks) {
        if (scope->isGlobalObject()) {
            ResolveType newResolveType = resolveType == UnresolvedProperty ? GlobalProperty : GlobalPropertyWithVarInjectionChecks;
            resolveType = newResolveType;
            ConcurrentJSLocker locker(codeBlock->m_lock);
            metadata.getPutInfo = GetPutInfo(metadata.getPutInfo.resolveMode(), newResolveType, metadata.getPutInfo.initializationMode());
        } else if (scope->isGlobalLexicalEnvironment()) {
            JSGlobalLexicalEnvironment* globalLexicalEnvironment = jsCast<JSGlobalLexicalEnvironment*>(scope);
            ResolveType newResolveType = resolveType == UnresolvedProperty ? GlobalLexicalVar : GlobalLexicalVarWithVarInjectionChecks;
            metadata.getPutInfo = GetPutInfo(metadata.getPutInfo.resolveMode(), newResolveType, metadata.getPutInfo.initializationMode());
            SymbolTableEntry entry = globalLexicalEnvironment->symbolTable()->get(ident.impl());
            ASSERT(!entry.isNull());
            ConcurrentJSLocker locker(codeBlock->m_lock);
            metadata.watchpointSet = entry.watchpointSet();
            metadata.operand = reinterpret_cast<uintptr_t>(globalLexicalEnvironment->variableAt(entry.scopeOffset()).slot());
        }
    }
    
    if (resolveType == GlobalProperty || resolveType == GlobalPropertyWithVarInjectionChecks) {
        VM& vm = exec->vm();
        JSGlobalObject* globalObject = codeBlock->globalObject();
        ASSERT(globalObject == scope || globalObject->varInjectionWatchpoint()->hasBeenInvalidated());
        if (!slot.isCacheablePut()
            || slot.base() != scope
            || scope != globalObject
            || !scope->structure(vm)->propertyAccessesAreCacheable())
            return;
        
        if (slot.type() == PutPropertySlot::NewProperty) {
            // Don't cache if we've done a transition. We want to detect the first replace so that we
            // can invalidate the watchpoint.
            return;
        }
        
        scope->structure(vm)->didCachePropertyReplacement(vm, slot.cachedOffset());

        ConcurrentJSLocker locker(codeBlock->m_lock);
        metadata.structure.set(vm, codeBlock, scope->structure(vm));
        metadata.operand = slot.cachedOffset();
    }
}

inline void tryCacheGetFromScopeGlobal(
    ExecState* exec, VM& vm, OpGetFromScope& bytecode, JSObject* scope, PropertySlot& slot, const Identifier& ident)
{
    auto& metadata = bytecode.metadata(exec);
    ResolveType resolveType = metadata.getPutInfo.resolveType();

    if (resolveType == UnresolvedProperty || resolveType == UnresolvedPropertyWithVarInjectionChecks) {
        if (scope->isGlobalObject()) {
            ResolveType newResolveType = resolveType == UnresolvedProperty ? GlobalProperty : GlobalPropertyWithVarInjectionChecks;
            resolveType = newResolveType; // Allow below caching mechanism to kick in.
            ConcurrentJSLocker locker(exec->codeBlock()->m_lock);
            metadata.getPutInfo = GetPutInfo(metadata.getPutInfo.resolveMode(), newResolveType, metadata.getPutInfo.initializationMode());
        } else if (scope->isGlobalLexicalEnvironment()) {
            JSGlobalLexicalEnvironment* globalLexicalEnvironment = jsCast<JSGlobalLexicalEnvironment*>(scope);
            ResolveType newResolveType = resolveType == UnresolvedProperty ? GlobalLexicalVar : GlobalLexicalVarWithVarInjectionChecks;
            SymbolTableEntry entry = globalLexicalEnvironment->symbolTable()->get(ident.impl());
            ASSERT(!entry.isNull());
            ConcurrentJSLocker locker(exec->codeBlock()->m_lock);
            metadata.getPutInfo = GetPutInfo(metadata.getPutInfo.resolveMode(), newResolveType, metadata.getPutInfo.initializationMode());
            metadata.watchpointSet = entry.watchpointSet();
            metadata.operand = reinterpret_cast<uintptr_t>(globalLexicalEnvironment->variableAt(entry.scopeOffset()).slot());
        }
    }

    // Covers implicit globals. Since they don't exist until they first execute, we didn't know how to cache them at compile time.
    if (resolveType == GlobalProperty || resolveType == GlobalPropertyWithVarInjectionChecks) {
        CodeBlock* codeBlock = exec->codeBlock();
        JSGlobalObject* globalObject = codeBlock->globalObject();
        ASSERT(scope == globalObject || globalObject->varInjectionWatchpoint()->hasBeenInvalidated());
        if (slot.isCacheableValue() && slot.slotBase() == scope && scope == globalObject && scope->structure(vm)->propertyAccessesAreCacheable()) {
            Structure* structure = scope->structure(vm);
            {
                ConcurrentJSLocker locker(codeBlock->m_lock);
                metadata.structure.set(vm, codeBlock, structure);
                metadata.operand = slot.cachedOffset();
            }
            structure->startWatchingPropertyForReplacements(vm, slot.cachedOffset());
        }
    }
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

static ALWAYS_INLINE void putDirectWithReify(VM& vm, ExecState* exec, JSObject* baseObject, PropertyName propertyName, JSValue value, PutPropertySlot& slot, Structure** result = nullptr)
{
    auto scope = DECLARE_THROW_SCOPE(vm);
    if (baseObject->inherits<JSFunction>(vm)) {
        jsCast<JSFunction*>(baseObject)->reifyLazyPropertyIfNeeded(vm, exec, propertyName);
        RETURN_IF_EXCEPTION(scope, void());
    }
    if (result)
        *result = baseObject->structure(vm);
    scope.release();
    baseObject->putDirect(vm, propertyName, value, slot);
}

static ALWAYS_INLINE void putDirectAccessorWithReify(VM& vm, ExecState* exec, JSObject* baseObject, PropertyName propertyName, GetterSetter* accessor, unsigned attribute)
{
    auto scope = DECLARE_THROW_SCOPE(vm);
    if (baseObject->inherits<JSFunction>(vm)) {
        jsCast<JSFunction*>(baseObject)->reifyLazyPropertyIfNeeded(vm, exec, propertyName);
        RETURN_IF_EXCEPTION(scope, void());
    }
    scope.release();
    baseObject->putDirectAccessor(exec, propertyName, accessor, attribute);
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

class ExecState;
struct Instruction;

#define SLOW_PATH
    
#define SLOW_PATH_DECL(name) \
extern "C" SlowPathReturnType SLOW_PATH name(ExecState* exec, const Instruction* pc)
    
#define SLOW_PATH_HIDDEN_DECL(name) \
SLOW_PATH_DECL(name) WTF_INTERNAL
    
SLOW_PATH_HIDDEN_DECL(slow_path_call_arityCheck);
SLOW_PATH_HIDDEN_DECL(slow_path_construct_arityCheck);
SLOW_PATH_HIDDEN_DECL(slow_path_create_direct_arguments);
SLOW_PATH_HIDDEN_DECL(slow_path_create_scoped_arguments);
SLOW_PATH_HIDDEN_DECL(slow_path_create_cloned_arguments);
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
SLOW_PATH_HIDDEN_DECL(slow_path_is_object);
SLOW_PATH_HIDDEN_DECL(slow_path_is_object_or_null);
SLOW_PATH_HIDDEN_DECL(slow_path_is_function);
SLOW_PATH_HIDDEN_DECL(slow_path_in_by_id);
SLOW_PATH_HIDDEN_DECL(slow_path_in_by_val);
SLOW_PATH_HIDDEN_DECL(slow_path_del_by_val);
SLOW_PATH_HIDDEN_DECL(slow_path_strcat);
SLOW_PATH_HIDDEN_DECL(slow_path_to_primitive);
SLOW_PATH_HIDDEN_DECL(slow_path_get_enumerable_length);
SLOW_PATH_HIDDEN_DECL(slow_path_has_generic_property);
SLOW_PATH_HIDDEN_DECL(slow_path_has_structure_property);
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
SLOW_PATH_HIDDEN_DECL(slow_path_create_rest);
SLOW_PATH_HIDDEN_DECL(slow_path_get_by_id_with_this);
SLOW_PATH_HIDDEN_DECL(slow_path_get_by_val_with_this);
SLOW_PATH_HIDDEN_DECL(slow_path_put_by_id_with_this);
SLOW_PATH_HIDDEN_DECL(slow_path_put_by_val_with_this);
SLOW_PATH_HIDDEN_DECL(slow_path_define_data_property);
SLOW_PATH_HIDDEN_DECL(slow_path_define_accessor_property);
SLOW_PATH_HIDDEN_DECL(slow_path_throw_static_error);
SLOW_PATH_HIDDEN_DECL(slow_path_new_array_with_spread);
SLOW_PATH_HIDDEN_DECL(slow_path_new_array_buffer);
SLOW_PATH_HIDDEN_DECL(slow_path_spread);

using SlowPathFunction = SlowPathReturnType(SLOW_PATH *)(ExecState*, const Instruction*);

} // namespace JSC
