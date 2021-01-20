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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "BytecodeStructs.h"
#include "CommonSlowPaths.h"

namespace JSC {

namespace CommonSlowPaths {

inline void tryCachePutToScopeGlobal(
    JSGlobalObject* globalObject, CodeBlock* codeBlock, OpPutToScope& bytecode, JSObject* scope,
    PutPropertySlot& slot, const Identifier& ident)
{
    // Covers implicit globals. Since they don't exist until they first execute, we didn't know how to cache them at compile time.
    auto& metadata = bytecode.metadata(codeBlock);
    ResolveType resolveType = metadata.m_getPutInfo.resolveType();

    switch (resolveType) {
    case UnresolvedProperty:
    case UnresolvedPropertyWithVarInjectionChecks: {
        if (scope->isGlobalObject()) {
            ResolveType newResolveType = needsVarInjectionChecks(resolveType) ? GlobalPropertyWithVarInjectionChecks : GlobalProperty;
            resolveType = newResolveType; // Allow below caching mechanism to kick in.
            ConcurrentJSLocker locker(codeBlock->m_lock);
            metadata.m_getPutInfo = GetPutInfo(metadata.m_getPutInfo.resolveMode(), newResolveType, metadata.m_getPutInfo.initializationMode(), metadata.m_getPutInfo.ecmaMode());
            break;
        }
        FALLTHROUGH;
    }
    case GlobalProperty:
    case GlobalPropertyWithVarInjectionChecks: {
        // Global Lexical Binding Epoch is changed. Update op_get_from_scope from GlobalProperty to GlobalLexicalVar.
        if (scope->isGlobalLexicalEnvironment()) {
            JSGlobalLexicalEnvironment* globalLexicalEnvironment = jsCast<JSGlobalLexicalEnvironment*>(scope);
            ResolveType newResolveType = needsVarInjectionChecks(resolveType) ? GlobalLexicalVarWithVarInjectionChecks : GlobalLexicalVar;
            SymbolTableEntry entry = globalLexicalEnvironment->symbolTable()->get(ident.impl());
            ASSERT(!entry.isNull());
            ConcurrentJSLocker locker(codeBlock->m_lock);
            metadata.m_getPutInfo = GetPutInfo(metadata.m_getPutInfo.resolveMode(), newResolveType, metadata.m_getPutInfo.initializationMode(), metadata.m_getPutInfo.ecmaMode());
            metadata.m_watchpointSet = entry.watchpointSet();
            metadata.m_operand = reinterpret_cast<uintptr_t>(globalLexicalEnvironment->variableAt(entry.scopeOffset()).slot());
            return;
        }
        break;
    }
    default:
        return;
    }

    if (resolveType == GlobalProperty || resolveType == GlobalPropertyWithVarInjectionChecks) {
        VM& vm = getVM(globalObject);
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
        metadata.m_structure.set(vm, codeBlock, scope->structure(vm));
        metadata.m_operand = slot.cachedOffset();
    }
}

inline void tryCacheGetFromScopeGlobal(
    JSGlobalObject* globalObject, CodeBlock* codeBlock, VM& vm, OpGetFromScope& bytecode, JSObject* scope, PropertySlot& slot, const Identifier& ident)
{
    auto& metadata = bytecode.metadata(codeBlock);
    ResolveType resolveType = metadata.m_getPutInfo.resolveType();

    switch (resolveType) {
    case UnresolvedProperty:
    case UnresolvedPropertyWithVarInjectionChecks: {
        if (scope->isGlobalObject()) {
            ResolveType newResolveType = needsVarInjectionChecks(resolveType) ? GlobalPropertyWithVarInjectionChecks : GlobalProperty;
            resolveType = newResolveType; // Allow below caching mechanism to kick in.
            ConcurrentJSLocker locker(codeBlock->m_lock);
            metadata.m_getPutInfo = GetPutInfo(metadata.m_getPutInfo.resolveMode(), newResolveType, metadata.m_getPutInfo.initializationMode(), metadata.m_getPutInfo.ecmaMode());
            break;
        }
        FALLTHROUGH;
    }
    case GlobalProperty:
    case GlobalPropertyWithVarInjectionChecks: {
        // Global Lexical Binding Epoch is changed. Update op_get_from_scope from GlobalProperty to GlobalLexicalVar.
        if (scope->isGlobalLexicalEnvironment()) {
            JSGlobalLexicalEnvironment* globalLexicalEnvironment = jsCast<JSGlobalLexicalEnvironment*>(scope);
            ResolveType newResolveType = needsVarInjectionChecks(resolveType) ? GlobalLexicalVarWithVarInjectionChecks : GlobalLexicalVar;
            SymbolTableEntry entry = globalLexicalEnvironment->symbolTable()->get(ident.impl());
            ASSERT(!entry.isNull());
            ConcurrentJSLocker locker(codeBlock->m_lock);
            metadata.m_getPutInfo = GetPutInfo(metadata.m_getPutInfo.resolveMode(), newResolveType, metadata.m_getPutInfo.initializationMode(), metadata.m_getPutInfo.ecmaMode());
            metadata.m_watchpointSet = entry.watchpointSet();
            metadata.m_operand = reinterpret_cast<uintptr_t>(globalLexicalEnvironment->variableAt(entry.scopeOffset()).slot());
            return;
        }
        break;
    }
    default:
        return;
    }

    // Covers implicit globals. Since they don't exist until they first execute, we didn't know how to cache them at compile time.
    if (resolveType == GlobalProperty || resolveType == GlobalPropertyWithVarInjectionChecks) {
        ASSERT(scope == globalObject || globalObject->varInjectionWatchpoint()->hasBeenInvalidated());
        if (slot.isCacheableValue() && slot.slotBase() == scope && scope == globalObject && scope->structure(vm)->propertyAccessesAreCacheable()) {
            Structure* structure = scope->structure(vm);
            {
                ConcurrentJSLocker locker(codeBlock->m_lock);
                metadata.m_structure.set(vm, codeBlock, structure);
                metadata.m_operand = slot.cachedOffset();
            }
            structure->startWatchingPropertyForReplacements(vm, slot.cachedOffset());
        }
    }
}

}} // namespace JSC::CommonSlowPaths
