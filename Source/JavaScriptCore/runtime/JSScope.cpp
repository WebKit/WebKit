/*
 * Copyright (C) 2012-2015 Apple Inc. All Rights Reserved.
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
#include "JSScope.h"

#include "JSGlobalObject.h"
#include "JSLexicalEnvironment.h"
#include "JSModuleEnvironment.h"
#include "JSModuleRecord.h"
#include "JSWithScope.h"
#include "JSCInlines.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(JSScope);

void JSScope::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    JSScope* thisObject = jsCast<JSScope*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
    visitor.append(&thisObject->m_next);
}

// Returns true if we found enough information to terminate optimization.
static inline bool abstractAccess(ExecState* exec, JSScope* scope, const Identifier& ident, GetOrPut getOrPut, size_t depth, bool& needsVarInjectionChecks, ResolveOp& op, InitializationMode initializationMode)
{
    if (scope->isJSLexicalEnvironment()) {
        JSLexicalEnvironment* lexicalEnvironment = jsCast<JSLexicalEnvironment*>(scope);
        if (ident == exec->propertyNames().arguments) {
            // We know the property will be at this lexical environment scope, but we don't know how to cache it.
            op = ResolveOp(Dynamic, 0, 0, 0, 0, 0);
            return true;
        }

        SymbolTableEntry entry = lexicalEnvironment->symbolTable()->get(ident.impl());
        if (entry.isReadOnly() && getOrPut == Put) {
            // We know the property will be at this lexical environment scope, but we don't know how to cache it.
            op = ResolveOp(Dynamic, 0, 0, 0, 0, 0);
            return true;
        }

        if (!entry.isNull()) {
            op = ResolveOp(makeType(ClosureVar, needsVarInjectionChecks), depth, 0, lexicalEnvironment, entry.watchpointSet(), entry.scopeOffset().offset());
            return true;
        }

        if (scope->type() == ModuleEnvironmentType) {
            JSModuleEnvironment* moduleEnvironment = jsCast<JSModuleEnvironment*>(scope);
            JSModuleRecord* moduleRecord = moduleEnvironment->moduleRecord();
            JSModuleRecord::Resolution resolution = moduleRecord->resolveImport(exec, ident);
            if (resolution.type == JSModuleRecord::Resolution::Type::Resolved) {
                JSModuleRecord* importedRecord = resolution.moduleRecord;
                JSModuleEnvironment* importedEnvironment = importedRecord->moduleEnvironment();
                SymbolTableEntry entry = importedEnvironment->symbolTable()->get(resolution.localName.impl());
                ASSERT(!entry.isNull());
                op = ResolveOp(makeType(ModuleVar, needsVarInjectionChecks), depth, 0, importedEnvironment, entry.watchpointSet(), entry.scopeOffset().offset(), resolution.localName.impl());
                return true;
            }
        }

        if (lexicalEnvironment->symbolTable()->usesNonStrictEval())
            needsVarInjectionChecks = true;
        return false;
    }

    if (scope->isGlobalLexicalEnvironment()) {
        JSGlobalLexicalEnvironment* globalLexicalEnvironment = jsCast<JSGlobalLexicalEnvironment*>(scope);
        SymbolTableEntry entry = globalLexicalEnvironment->symbolTable()->get(ident.impl());
        if (!entry.isNull()) {
            if (getOrPut == Put && entry.isReadOnly() && initializationMode != Initialization) {
                // We know the property will be at global lexical environment, but we don't know how to cache it.
                op = ResolveOp(Dynamic, 0, 0, 0, 0, 0);
                return true;
            }

            // We can try to force const Initialization to always go down the fast path. It is provably impossible to construct
            // a program that needs a var injection check here. You can convince yourself of this as follows:
            // Any other let/const/class would be a duplicate of this in the global scope, so we would never get here in that situation.
            // Also, if we had an eval in the global scope that defined a const, it would also be a duplicate of this const, and so it would
            // also throw an error. Therefore, we're *the only* thing that can assign to this "const" slot for the first (and only) time. Also, 
            // we will never have a Dynamic ResolveType here because if we were inside a "with" statement, that would mean the "const" definition 
            // isn't a global, it would be a local to the "with" block. 
            // We still need to make the slow path correct for when we need to fire a watchpoint.
            ResolveType resolveType = initializationMode == Initialization ? GlobalLexicalVar : makeType(GlobalLexicalVar, needsVarInjectionChecks);
            op = ResolveOp(
                resolveType, depth, 0, 0, entry.watchpointSet(),
                reinterpret_cast<uintptr_t>(globalLexicalEnvironment->variableAt(entry.scopeOffset()).slot()));
            return true;
        }

        return false;
    }

    if (scope->isGlobalObject()) {
        JSGlobalObject* globalObject = jsCast<JSGlobalObject*>(scope);
        SymbolTableEntry entry = globalObject->symbolTable()->get(ident.impl());
        if (!entry.isNull()) {
            if (getOrPut == Put && entry.isReadOnly()) {
                // We know the property will be at global scope, but we don't know how to cache it.
                op = ResolveOp(Dynamic, 0, 0, 0, 0, 0);
                return true;
            }

            op = ResolveOp(
                makeType(GlobalVar, needsVarInjectionChecks), depth, 0, 0, entry.watchpointSet(),
                reinterpret_cast<uintptr_t>(globalObject->variableAt(entry.scopeOffset()).slot()));
            return true;
        }

        PropertySlot slot(globalObject, PropertySlot::InternalMethodType::VMInquiry);
        bool hasOwnProperty = globalObject->getOwnPropertySlot(globalObject, exec, ident, slot);
        if (!hasOwnProperty) {
            op = ResolveOp(makeType(UnresolvedProperty, needsVarInjectionChecks), 0, 0, 0, 0, 0);
            return true;
        }

        if (!slot.isCacheableValue()
            || !globalObject->structure()->propertyAccessesAreCacheable()
            || (globalObject->structure()->hasReadOnlyOrGetterSetterPropertiesExcludingProto() && getOrPut == Put)) {
            // We know the property will be at global scope, but we don't know how to cache it.
            ASSERT(!scope->next());
            op = ResolveOp(makeType(GlobalProperty, needsVarInjectionChecks), 0, 0, 0, 0, 0);
            return true;
        }

        
        WatchpointState state = globalObject->structure()->ensurePropertyReplacementWatchpointSet(exec->vm(), slot.cachedOffset())->state();
        if (state == IsWatched && getOrPut == Put) {
            // The field exists, but because the replacement watchpoint is still intact. This is
            // kind of dangerous. We have two options:
            // 1) Invalidate the watchpoint set. That would work, but it's possible that this code
            //    path never executes - in which case this would be unwise.
            // 2) Have the invalidation happen at run-time. All we have to do is leave the code
            //    uncached. The only downside is slightly more work when this does execute.
            // We go with option (2) here because it seems less evil.
            op = ResolveOp(makeType(GlobalProperty, needsVarInjectionChecks), depth, 0, 0, 0, 0);
        } else
            op = ResolveOp(makeType(GlobalProperty, needsVarInjectionChecks), depth, globalObject->structure(), 0, 0, slot.cachedOffset());
        return true;
    }

    op = ResolveOp(Dynamic, 0, 0, 0, 0, 0);
    return true;
}

JSObject* JSScope::objectAtScope(JSScope* scope)
{
    JSObject* object = scope;
    if (object->type() == WithScopeType)
        return jsCast<JSWithScope*>(object)->object();

    return object;
}

// When an exception occurs, the result of isUnscopable becomes false.
static inline bool isUnscopable(ExecState* exec, JSScope* scope, JSObject* object, const Identifier& ident)
{
    if (scope->type() != WithScopeType)
        return false;

    JSValue unscopables = object->get(exec, exec->propertyNames().unscopablesSymbol);
    if (exec->hadException())
        return false;
    if (!unscopables.isObject())
        return false;
    JSValue blocked = jsCast<JSObject*>(unscopables)->get(exec, ident);
    if (exec->hadException())
        return false;

    return blocked.toBoolean(exec);
}

JSObject* JSScope::resolve(ExecState* exec, JSScope* scope, const Identifier& ident)
{
    ScopeChainIterator end = scope->end();
    ScopeChainIterator it = scope->begin();
    while (1) {
        JSScope* scope = it.scope();
        JSObject* object = it.get();

        if (++it == end) // Global scope.
            return object;

        if (object->hasProperty(exec, ident)) {
            if (!isUnscopable(exec, scope, object, ident))
                return object;
            ASSERT_WITH_MESSAGE(!exec->hadException(), "When an exception occurs, the result of isUnscopable becomes false");
        }
    }
}

ResolveOp JSScope::abstractResolve(ExecState* exec, size_t depthOffset, JSScope* scope, const Identifier& ident, GetOrPut getOrPut, ResolveType unlinkedType, InitializationMode initializationMode)
{
    ResolveOp op(Dynamic, 0, 0, 0, 0, 0);
    if (unlinkedType == Dynamic)
        return op;

    bool needsVarInjectionChecks = JSC::needsVarInjectionChecks(unlinkedType);
    size_t depth = depthOffset;
    for (; scope; scope = scope->next()) {
        if (abstractAccess(exec, scope, ident, getOrPut, depth, needsVarInjectionChecks, op, initializationMode))
            break;
        ++depth;
    }

    return op;
}

void JSScope::collectVariablesUnderTDZ(JSScope* scope, VariableEnvironment& result)
{
    for (; scope; scope = scope->next()) {
        if (!scope->isLexicalScope() && !scope->isGlobalLexicalEnvironment())
            continue;

        if (scope->isModuleScope()) {
            JSModuleRecord* moduleRecord = jsCast<JSModuleEnvironment*>(scope)->moduleRecord();
            for (const auto& pair : moduleRecord->importEntries())
                result.add(pair.key);
        }

        SymbolTable* symbolTable = jsCast<JSSymbolTableObject*>(scope)->symbolTable();
        ASSERT(symbolTable->scopeType() == SymbolTable::ScopeType::LexicalScope || symbolTable->scopeType() == SymbolTable::ScopeType::GlobalLexicalScope);
        ConcurrentJITLocker locker(symbolTable->m_lock);
        for (auto end = symbolTable->end(locker), iter = symbolTable->begin(locker); iter != end; ++iter)
            result.add(iter->key);
    }
}

bool JSScope::isVarScope()
{
    if (type() != LexicalEnvironmentType)
        return false;
    return jsCast<JSLexicalEnvironment*>(this)->symbolTable()->scopeType() == SymbolTable::ScopeType::VarScope;
}

bool JSScope::isLexicalScope()
{
    if (!isJSLexicalEnvironment())
        return false;
    return jsCast<JSLexicalEnvironment*>(this)->symbolTable()->scopeType() == SymbolTable::ScopeType::LexicalScope;
}

bool JSScope::isModuleScope()
{
    return type() == ModuleEnvironmentType;
}

bool JSScope::isCatchScope()
{
    if (type() != LexicalEnvironmentType)
        return false;
    return jsCast<JSLexicalEnvironment*>(this)->symbolTable()->scopeType() == SymbolTable::ScopeType::CatchScope;
}

bool JSScope::isFunctionNameScopeObject()
{
    if (type() != LexicalEnvironmentType)
        return false;
    return jsCast<JSLexicalEnvironment*>(this)->symbolTable()->scopeType() == SymbolTable::ScopeType::FunctionNameScope;
}

bool JSScope::isNestedLexicalScope()
{
    if (!isJSLexicalEnvironment())
        return false;
    return jsCast<JSLexicalEnvironment*>(this)->symbolTable()->isNestedLexicalScope();
}

JSScope* JSScope::constantScopeForCodeBlock(ResolveType type, CodeBlock* codeBlock)
{
    switch (type) {
    case GlobalProperty:
    case GlobalVar:
    case GlobalPropertyWithVarInjectionChecks:
    case GlobalVarWithVarInjectionChecks:
        return codeBlock->globalObject();
    case GlobalLexicalVarWithVarInjectionChecks:
    case GlobalLexicalVar:
        return codeBlock->globalObject()->globalLexicalEnvironment();
    default:
        return nullptr;
    }

    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

} // namespace JSC
