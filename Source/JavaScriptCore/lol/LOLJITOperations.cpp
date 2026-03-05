/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "LOLJITOperations.h"

#if ENABLE(JIT) && USE(JSVALUE64)

#include "ArithProfile.h"
#include "ArrayConstructor.h"
#include "CacheableIdentifierInlines.h"
#include "CodeBlockInlines.h"
#include "CommonSlowPathsInlines.h"
#include "DFGDriver.h"
#include "DFGOSREntry.h"
#include "DFGThunks.h"
#include "Debugger.h"
#include "EnsureStillAliveHere.h"
#include "ExceptionFuzz.h"
#include "FrameTracers.h"
#include "GetterSetter.h"
#include "ICStats.h"
#include "InlineCacheCompiler.h"
#include "Interpreter.h"
#include "JIT.h"
#include "JITExceptions.h"
#include "JITThunks.h"
#include "JITToDFGDeferredCompilationCallback.h"
#include "JITWorklist.h"
#include "JSArrayIterator.h"
#include "JSAsyncFunction.h"
#include "JSAsyncGenerator.h"
#include "JSAsyncGeneratorFunction.h"
#include "JSBoundFunction.h"
#include "JSCInlines.h"
#include "JSCPtrTag.h"
#include "JSGeneratorFunction.h"
#include "JSGlobalObjectFunctions.h"
#include "JSInternalPromise.h"
#include "JSLexicalEnvironment.h"
#include "JSRemoteFunction.h"
#include "JSWithScope.h"
#include "LLIntEntrypoint.h"
#include "MegamorphicCache.h"
#include "ObjectConstructor.h"
#include "PropertyName.h"
#include "RegExpObject.h"
#include "RepatchInlines.h"
#include "ShadowChicken.h"
#include "StructureStubInfo.h"
#include "SuperSampler.h"
#include "ThunkGenerators.h"
#include "TypeProfilerLog.h"

namespace JSC::LOL {

JSC_DEFINE_JIT_OPERATION(operationResolveScopeForLOL, EncodedJSValue, (CallFrame* callFrame, unsigned bytecodeOffset, JSScope* environment))
{
    CodeBlock* codeBlock = callFrame->codeBlock();
    JSGlobalObject* globalObject = codeBlock->globalObject();
    VM& vm = globalObject->vm();
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    const JSInstruction* pc = codeBlock->instructionAt(BytecodeIndex(bytecodeOffset));
    auto bytecode = pc->as<OpResolveScope>();
    const Identifier& ident = codeBlock->identifier(bytecode.m_var);
    JSObject* resolvedScope = JSScope::resolve(globalObject, environment, ident);
    // Proxy can throw an error here, e.g. Proxy in with statement's @unscopables.
    OPERATION_RETURN_IF_EXCEPTION(scope, encodedJSValue());

    auto& metadata = bytecode.metadata(codeBlock);
    ResolveType resolveType = metadata.m_resolveType;

    // ModuleVar does not keep the scope register value alive in DFG.
    ASSERT(resolveType != ModuleVar);

    switch (resolveType) {
    case GlobalProperty:
    case GlobalPropertyWithVarInjectionChecks:
    case UnresolvedProperty:
    case UnresolvedPropertyWithVarInjectionChecks: {
        if (resolvedScope->isGlobalObject()) {
            JSGlobalObject* globalObject = jsCast<JSGlobalObject*>(resolvedScope);
            bool hasProperty = globalObject->hasProperty(globalObject, ident);
            OPERATION_RETURN_IF_EXCEPTION(scope, encodedJSValue());
            if (hasProperty) {
                ConcurrentJSLocker locker(codeBlock->m_lock);
                metadata.m_resolveType = needsVarInjectionChecks(resolveType) ? GlobalPropertyWithVarInjectionChecks : GlobalProperty;
                metadata.m_globalObject.set(vm, codeBlock, globalObject);
                metadata.m_globalLexicalBindingEpoch = globalObject->globalLexicalBindingEpoch();
            }
        } else if (resolvedScope->isGlobalLexicalEnvironment()) {
            JSGlobalLexicalEnvironment* globalLexicalEnvironment = jsCast<JSGlobalLexicalEnvironment*>(resolvedScope);
            ConcurrentJSLocker locker(codeBlock->m_lock);
            metadata.m_resolveType = needsVarInjectionChecks(resolveType) ? GlobalLexicalVarWithVarInjectionChecks : GlobalLexicalVar;
            metadata.m_globalLexicalEnvironment.set(vm, codeBlock, globalLexicalEnvironment);
        }
        break;
    }
    default:
        break;
    }

    OPERATION_RETURN(scope, JSValue::encode(resolvedScope));
}

JSC_DEFINE_JIT_OPERATION(operationGetFromScopeForLOL, EncodedJSValue, (CallFrame* callFrame, unsigned bytecodeOffset, JSObject* environment))
{
    CodeBlock* codeBlock = callFrame->codeBlock();
    JSGlobalObject* globalObject = codeBlock->globalObject();
    VM& vm = globalObject->vm();
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    const JSInstruction* pc = codeBlock->instructionAt(BytecodeIndex(bytecodeOffset));
    auto bytecode = pc->as<OpGetFromScope>();
    const Identifier& ident = codeBlock->identifier(bytecode.m_var);
    GetPutInfo& getPutInfo = bytecode.metadata(codeBlock).m_getPutInfo;

    // ModuleVar is always converted to ClosureVar for get_from_scope.
    ASSERT(getPutInfo.resolveType() != ModuleVar);

    OPERATION_RETURN(scope, JSValue::encode(environment->getPropertySlot(globalObject, ident, [&] (bool found, PropertySlot& slot) -> JSValue {
        if (!found) {
            if (getPutInfo.resolveMode() == ThrowIfNotFound)
                throwException(globalObject, scope, createUndefinedVariableError(globalObject, ident));
            return jsUndefined();
        }

        JSValue result = JSValue();
        if (environment->isGlobalLexicalEnvironment()) {
            // When we can't statically prove we need a TDZ check, we must perform the check on the slow path.
            result = slot.getValue(globalObject, ident);
            if (result == jsTDZValue()) {
                throwException(globalObject, scope, createTDZError(globalObject, ident.string()));
                return jsUndefined();
            }
        }

        CommonSlowPaths::tryCacheGetFromScopeGlobal(globalObject, codeBlock, vm, bytecode, environment, slot, ident);

        if (!result)
            return slot.getValue(globalObject, ident);
        return result;
    })));
}

JSC_DEFINE_JIT_OPERATION(operationPutToScopeForLOL, void, (CallFrame* callFrame, unsigned bytecodeOffset, JSObject* jsScope, JSValue value))
{
    CodeBlock* codeBlock = callFrame->codeBlock();
    JSGlobalObject* globalObject = codeBlock->globalObject();
    VM& vm = globalObject->vm();
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    const JSInstruction* pc = codeBlock->instructionAt(BytecodeIndex(bytecodeOffset));
    auto bytecode = pc->as<OpPutToScope>();
    auto& metadata = bytecode.metadata(codeBlock);

    const Identifier& ident = codeBlock->identifier(bytecode.m_var);
    GetPutInfo& getPutInfo = metadata.m_getPutInfo;

    // ModuleVar does not keep the scope register value alive in DFG.
    ASSERT(getPutInfo.resolveType() != ModuleVar);

    if (getPutInfo.resolveType() == ResolvedClosureVar) {
        JSLexicalEnvironment* environment = jsCast<JSLexicalEnvironment*>(jsScope);
        environment->variableAt(ScopeOffset(metadata.m_operand)).set(vm, environment, value);
        if (RefPtr set = metadata.m_watchpointSet)
            set->touch(vm, "Executed op_put_scope<ResolvedClosureVar>");
        OPERATION_RETURN(scope);
    }

    bool hasProperty = jsScope->hasProperty(globalObject, ident);
    OPERATION_RETURN_IF_EXCEPTION(scope);
    if (hasProperty
        && jsScope->isGlobalLexicalEnvironment()
        && !isInitialization(getPutInfo.initializationMode())) {
        // When we can't statically prove we need a TDZ check, we must perform the check on the slow path.
        PropertySlot slot(jsScope, PropertySlot::InternalMethodType::Get);
        JSGlobalLexicalEnvironment::getOwnPropertySlot(jsScope, globalObject, ident, slot);
        if (slot.getValue(globalObject, ident) == jsTDZValue()) {
            throwException(globalObject, scope, createTDZError(globalObject, ident.string()));
            OPERATION_RETURN(scope);
        }
    }

    if (getPutInfo.resolveMode() == ThrowIfNotFound && !hasProperty) {
        throwException(globalObject, scope, createUndefinedVariableError(globalObject, ident));
        OPERATION_RETURN(scope);
    }

    PutPropertySlot slot(jsScope, getPutInfo.ecmaMode().isStrict(), PutPropertySlot::UnknownContext, isInitialization(getPutInfo.initializationMode()));
    jsScope->methodTable()->put(jsScope, globalObject, ident, value, slot);

    OPERATION_RETURN_IF_EXCEPTION(scope);

    CommonSlowPaths::tryCachePutToScopeGlobal(globalObject, codeBlock, bytecode, jsScope, slot, ident);
    OPERATION_RETURN(scope);
}

} // namespace JSC::LOL

#endif // ENABLE(JIT) && USE(JSVALUE64)
