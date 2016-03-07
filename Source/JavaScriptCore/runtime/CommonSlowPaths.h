/*
 * Copyright (C) 2011-2013, 2015 Apple Inc. All rights reserved.
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

#ifndef CommonSlowPaths_h
#define CommonSlowPaths_h

#include "CodeBlock.h"
#include "CodeSpecializationKind.h"
#include "ExceptionHelpers.h"
#include "JSStackInlines.h"
#include "SlowPathReturnType.h"
#include "StackAlignment.h"
#include "Symbol.h"
#include "VM.h"
#include <wtf/StdLibExtras.h>

namespace JSC {

// The purpose of this namespace is to include slow paths that are shared
// between the interpreter and baseline JIT. They are written to be agnostic
// with respect to the slow-path calling convention, but they do rely on the
// JS code being executed more-or-less directly from bytecode (so the call
// frame layout is unmodified, making it potentially awkward to use these
// from any optimizing JIT, like the DFG).

namespace CommonSlowPaths {

struct ArityCheckData {
    unsigned paddedStackSpace;
    void* thunkToCall;
};

ALWAYS_INLINE int arityCheckFor(ExecState* exec, JSStack* stack, CodeSpecializationKind kind)
{
    JSFunction* callee = jsCast<JSFunction*>(exec->callee());
    ASSERT(!callee->isHostFunction());
    CodeBlock* newCodeBlock = callee->jsExecutable()->codeBlockFor(kind);
    int argumentCountIncludingThis = exec->argumentCountIncludingThis();
    
    ASSERT(argumentCountIncludingThis < newCodeBlock->numParameters());
    int frameSize = argumentCountIncludingThis + JSStack::CallFrameHeaderSize;
    int alignedFrameSizeForParameters = WTF::roundUpToMultipleOf(stackAlignmentRegisters(),
        newCodeBlock->numParameters() + JSStack::CallFrameHeaderSize);
    int paddedStackSpace = alignedFrameSizeForParameters - frameSize;

    if (!stack->ensureCapacityFor(exec->registers() - paddedStackSpace % stackAlignmentRegisters()))
        return -1;
    return paddedStackSpace;
}

inline bool opIn(ExecState* exec, JSValue propName, JSValue baseVal)
{
    if (!baseVal.isObject()) {
        exec->vm().throwException(exec, createInvalidInParameterError(exec, baseVal));
        return false;
    }

    JSObject* baseObj = asObject(baseVal);

    uint32_t i;
    if (propName.getUInt32(i))
        return baseObj->hasProperty(exec, i);

    auto property = propName.toPropertyKey(exec);
    if (exec->vm().exception())
        return false;
    return baseObj->hasProperty(exec, property);
}

inline void tryCachePutToScopeGlobal(
    ExecState* exec, CodeBlock* codeBlock, Instruction* pc, JSObject* scope,
    GetPutInfo getPutInfo, PutPropertySlot& slot, const Identifier& ident)
{
    // Covers implicit globals. Since they don't exist until they first execute, we didn't know how to cache them at compile time.
    ResolveType resolveType = getPutInfo.resolveType();
    if (resolveType != GlobalProperty && resolveType != GlobalPropertyWithVarInjectionChecks 
        && resolveType != UnresolvedProperty && resolveType != UnresolvedPropertyWithVarInjectionChecks)
        return;

    if (resolveType == UnresolvedProperty || resolveType == UnresolvedPropertyWithVarInjectionChecks) {
        if (JSGlobalLexicalEnvironment* globalLexicalEnvironment = jsDynamicCast<JSGlobalLexicalEnvironment*>(scope)) {
            ResolveType newResolveType = resolveType == UnresolvedProperty ? GlobalLexicalVar : GlobalLexicalVarWithVarInjectionChecks;
            pc[4].u.operand = GetPutInfo(getPutInfo.resolveMode(), newResolveType, getPutInfo.initializationMode()).operand();
            SymbolTableEntry entry = globalLexicalEnvironment->symbolTable()->get(ident.impl());
            ASSERT(!entry.isNull());
            pc[5].u.watchpointSet = entry.watchpointSet();
            pc[6].u.pointer = static_cast<void*>(globalLexicalEnvironment->variableAt(entry.scopeOffset()).slot());
        } else if (jsDynamicCast<JSGlobalObject*>(scope)) {
            ResolveType newResolveType = resolveType == UnresolvedProperty ? GlobalProperty : GlobalPropertyWithVarInjectionChecks;
            resolveType = newResolveType;
            getPutInfo = GetPutInfo(getPutInfo.resolveMode(), newResolveType, getPutInfo.initializationMode());
            pc[4].u.operand = getPutInfo.operand();
        }
    }
    
    if (resolveType == GlobalProperty || resolveType == GlobalPropertyWithVarInjectionChecks) {
        if (!slot.isCacheablePut()
            || slot.base() != scope
            || !scope->structure()->propertyAccessesAreCacheable())
            return;
        
        if (slot.type() == PutPropertySlot::NewProperty) {
            // Don't cache if we've done a transition. We want to detect the first replace so that we
            // can invalidate the watchpoint.
            return;
        }
        
        scope->structure()->didCachePropertyReplacement(exec->vm(), slot.cachedOffset());

        ConcurrentJITLocker locker(codeBlock->m_lock);
        pc[5].u.structure.set(exec->vm(), codeBlock, scope->structure());
        pc[6].u.operand = slot.cachedOffset();
    }
}

inline void tryCacheGetFromScopeGlobal(
    ExecState* exec, VM& vm, Instruction* pc, JSObject* scope, PropertySlot& slot, const Identifier& ident)
{
    GetPutInfo getPutInfo(pc[4].u.operand);
    ResolveType resolveType = getPutInfo.resolveType();

    if (resolveType == UnresolvedProperty || resolveType == UnresolvedPropertyWithVarInjectionChecks) {
        if (JSGlobalLexicalEnvironment* globalLexicalEnvironment = jsDynamicCast<JSGlobalLexicalEnvironment*>(scope)) {
            ResolveType newResolveType = resolveType == UnresolvedProperty ? GlobalLexicalVar : GlobalLexicalVarWithVarInjectionChecks;
            pc[4].u.operand = GetPutInfo(getPutInfo.resolveMode(), newResolveType, getPutInfo.initializationMode()).operand();
            SymbolTableEntry entry = globalLexicalEnvironment->symbolTable()->get(ident.impl());
            ASSERT(!entry.isNull());
            pc[5].u.watchpointSet = entry.watchpointSet();
            pc[6].u.pointer = static_cast<void*>(globalLexicalEnvironment->variableAt(entry.scopeOffset()).slot());
        } else if (jsDynamicCast<JSGlobalObject*>(scope)) {
            ResolveType newResolveType = resolveType == UnresolvedProperty ? GlobalProperty : GlobalPropertyWithVarInjectionChecks;
            resolveType = newResolveType; // Allow below caching mechanism to kick in.
            pc[4].u.operand = GetPutInfo(getPutInfo.resolveMode(), newResolveType, getPutInfo.initializationMode()).operand();
        }
    }

    // Covers implicit globals. Since they don't exist until they first execute, we didn't know how to cache them at compile time.
    if (slot.isCacheableValue() && slot.slotBase() == scope && scope->structure()->propertyAccessesAreCacheable()) {
        if (resolveType == GlobalProperty || resolveType == GlobalPropertyWithVarInjectionChecks) {
            CodeBlock* codeBlock = exec->codeBlock();
            Structure* structure = scope->structure(vm);
            {
                ConcurrentJITLocker locker(codeBlock->m_lock);
                pc[5].u.structure.set(exec->vm(), codeBlock, structure);
                pc[6].u.operand = slot.cachedOffset();
            }
            structure->startWatchingPropertyForReplacements(vm, slot.cachedOffset());
        }
    }
}

} // namespace CommonSlowPaths

class ExecState;
struct Instruction;

#define SLOW_PATH
    
#define SLOW_PATH_DECL(name) \
extern "C" SlowPathReturnType SLOW_PATH name(ExecState* exec, Instruction* pc)
    
#define SLOW_PATH_HIDDEN_DECL(name) \
SLOW_PATH_DECL(name) WTF_INTERNAL
    
SLOW_PATH_HIDDEN_DECL(slow_path_call_arityCheck);
SLOW_PATH_HIDDEN_DECL(slow_path_construct_arityCheck);
SLOW_PATH_HIDDEN_DECL(slow_path_create_direct_arguments);
SLOW_PATH_HIDDEN_DECL(slow_path_create_scoped_arguments);
SLOW_PATH_HIDDEN_DECL(slow_path_create_out_of_band_arguments);
SLOW_PATH_HIDDEN_DECL(slow_path_create_this);
SLOW_PATH_HIDDEN_DECL(slow_path_enter);
SLOW_PATH_HIDDEN_DECL(slow_path_get_callee);
SLOW_PATH_HIDDEN_DECL(slow_path_to_this);
SLOW_PATH_HIDDEN_DECL(slow_path_throw_tdz_error);
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
SLOW_PATH_HIDDEN_DECL(slow_path_negate);
SLOW_PATH_HIDDEN_DECL(slow_path_add);
SLOW_PATH_HIDDEN_DECL(slow_path_mul);
SLOW_PATH_HIDDEN_DECL(slow_path_sub);
SLOW_PATH_HIDDEN_DECL(slow_path_div);
SLOW_PATH_HIDDEN_DECL(slow_path_mod);
SLOW_PATH_HIDDEN_DECL(slow_path_lshift);
SLOW_PATH_HIDDEN_DECL(slow_path_rshift);
SLOW_PATH_HIDDEN_DECL(slow_path_urshift);
SLOW_PATH_HIDDEN_DECL(slow_path_unsigned);
SLOW_PATH_HIDDEN_DECL(slow_path_bitand);
SLOW_PATH_HIDDEN_DECL(slow_path_bitor);
SLOW_PATH_HIDDEN_DECL(slow_path_bitxor);
SLOW_PATH_HIDDEN_DECL(slow_path_typeof);
SLOW_PATH_HIDDEN_DECL(slow_path_is_object);
SLOW_PATH_HIDDEN_DECL(slow_path_is_object_or_null);
SLOW_PATH_HIDDEN_DECL(slow_path_is_function);
SLOW_PATH_HIDDEN_DECL(slow_path_in);
SLOW_PATH_HIDDEN_DECL(slow_path_del_by_val);
SLOW_PATH_HIDDEN_DECL(slow_path_strcat);
SLOW_PATH_HIDDEN_DECL(slow_path_to_primitive);
SLOW_PATH_HIDDEN_DECL(slow_path_get_enumerable_length);
SLOW_PATH_HIDDEN_DECL(slow_path_has_generic_property);
SLOW_PATH_HIDDEN_DECL(slow_path_has_structure_property);
SLOW_PATH_HIDDEN_DECL(slow_path_has_indexed_property);
SLOW_PATH_HIDDEN_DECL(slow_path_get_direct_pname);
SLOW_PATH_HIDDEN_DECL(slow_path_get_property_enumerator);
SLOW_PATH_HIDDEN_DECL(slow_path_next_structure_enumerator_pname);
SLOW_PATH_HIDDEN_DECL(slow_path_next_generic_enumerator_pname);
SLOW_PATH_HIDDEN_DECL(slow_path_to_index_string);
SLOW_PATH_HIDDEN_DECL(slow_path_profile_type_clear_log);
SLOW_PATH_HIDDEN_DECL(slow_path_assert);
SLOW_PATH_HIDDEN_DECL(slow_path_save);
SLOW_PATH_HIDDEN_DECL(slow_path_resume);
SLOW_PATH_HIDDEN_DECL(slow_path_create_lexical_environment);
SLOW_PATH_HIDDEN_DECL(slow_path_push_with_scope);
SLOW_PATH_HIDDEN_DECL(slow_path_resolve_scope);
SLOW_PATH_HIDDEN_DECL(slow_path_copy_rest);

} // namespace JSC

#endif // CommonSlowPaths_h
