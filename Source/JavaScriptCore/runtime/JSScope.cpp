/*
 * Copyright (C) 2012-2021 Apple Inc. All rights reserved.
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

#include "AbstractModuleRecord.h"
#include "DeferTermination.h"
#include "JSCInlines.h"
#include "JSGlobalLexicalEnvironment.h"
#include "JSLexicalEnvironment.h"
#include "JSModuleEnvironment.h"
#include "JSWithScope.h"
#include "StrictEvalActivation.h"
#include "TopExceptionScope.h"
#include "VMTrapsInlines.h"
#include "VariableEnvironment.h"

namespace JSC {

// Returns true if we found enough information to terminate optimization.
static inline bool abstractAccess(JSGlobalObject* globalObject, JSObject* scopeObject, const Identifier& ident, GetOrPut getOrPut, size_t depth, bool& needsVarInjectionChecks, ResolveOp& op, InitializationMode initializationMode)
{
    VM& vm = globalObject->vm();
    DeferTerminationForAWhile deferScope(vm);

    if (scopeObject->isJSLexicalEnvironment()) {
        JSLexicalEnvironment* lexicalEnvironment = uncheckedDowncast<JSLexicalEnvironment>(scopeObject);

        SymbolTable* symbolTable = lexicalEnvironment->symbolTable();
        {
            ConcurrentJSLocker locker(symbolTable->m_lock);
            auto iter = symbolTable->find(locker, ident.impl());
            if (iter != symbolTable->end(locker)) {
                SymbolTableEntry& entry = iter->value;
                ASSERT(!entry.isNull());
                if (entry.isReadOnly() && getOrPut == Put) {
                    // We know the property will be at this lexical environment scope, but we don't know how to cache it.
                    op = ResolveOp(Dynamic, 0, nullptr, nullptr, nullptr, 0);
                    return true;
                }

                op = ResolveOp(makeType(ClosureVar, needsVarInjectionChecks), depth, nullptr, lexicalEnvironment, entry.watchpointSet(), entry.scopeOffset().offset());
                return true;
            }
        }

        if (scopeObject->type() == ModuleEnvironmentType) {
            JSModuleEnvironment* moduleEnvironment = uncheckedDowncast<JSModuleEnvironment>(scopeObject);
            AbstractModuleRecord* moduleRecord = moduleEnvironment->moduleRecord();
            auto catchScope = DECLARE_TOP_EXCEPTION_SCOPE(vm);
            AbstractModuleRecord::Resolution resolution = moduleRecord->resolveImport(globalObject, ident);
            catchScope.releaseAssertNoException();
            if (resolution.type == AbstractModuleRecord::Resolution::Type::Resolved) {
                AbstractModuleRecord* importedRecord = resolution.moduleRecord;
                JSModuleEnvironment* importedEnvironment = importedRecord->moduleEnvironment();
                SymbolTable* symbolTable = importedEnvironment->symbolTable();
                ConcurrentJSLocker locker(symbolTable->m_lock);
                auto iter = symbolTable->find(locker, resolution.localName.impl());
                ASSERT(iter != symbolTable->end(locker));
                SymbolTableEntry& entry = iter->value;
                ASSERT(!entry.isNull());
                op = ResolveOp(makeType(ModuleVar, needsVarInjectionChecks), depth, nullptr, importedEnvironment, entry.watchpointSet(), entry.scopeOffset().offset(), resolution.localName.impl());
                return true;
            }
        }

        if (symbolTable->usesSloppyEval())
            needsVarInjectionChecks = true;
        return false;
    }

    if (scopeObject->isGlobalLexicalEnvironment()) {
        JSGlobalLexicalEnvironment* globalLexicalEnvironment = uncheckedDowncast<JSGlobalLexicalEnvironment>(scopeObject);
        SymbolTable* symbolTable = globalLexicalEnvironment->symbolTable();
        ConcurrentJSLocker locker(symbolTable->m_lock);
        auto iter = symbolTable->find(locker, ident.impl());
        if (iter != symbolTable->end(locker)) {
            SymbolTableEntry& entry = iter->value;
            ASSERT(!entry.isNull());
            if (getOrPut == Put && entry.isReadOnly() && !isInitialization(initializationMode)) {
                // We know the property will be at global lexical environment, but we don't know how to cache it.
                op = ResolveOp(Dynamic, 0, nullptr, nullptr, nullptr, 0);
                return true;
            }

            ResolveType resolveType = initializationMode == InitializationMode::ConstInitialization ? GlobalLexicalVar : makeType(GlobalLexicalVar, needsVarInjectionChecks);
            op = ResolveOp(
                resolveType, depth, nullptr, nullptr, entry.watchpointSet(),
                reinterpret_cast<uintptr_t>(globalLexicalEnvironment->variableAt(entry.scopeOffset()).slot()));
            return true;
        }

        return false;
    }

    if (scopeObject->isGlobalObject()) {
        JSGlobalObject* globalObjectScope = uncheckedDowncast<JSGlobalObject>(scopeObject);
        {
            SymbolTable* symbolTable = globalObjectScope->symbolTable();
            ConcurrentJSLocker locker(symbolTable->m_lock);
            auto iter = symbolTable->find(locker, ident.impl());
            if (iter != symbolTable->end(locker)) {
                SymbolTableEntry& entry = iter->value;
                ASSERT(!entry.isNull());
                if (getOrPut == Put && entry.isReadOnly()) {
                    // We know the property will be at global scope, but we don't know how to cache it.
                    op = ResolveOp(Dynamic, 0, nullptr, nullptr, nullptr, 0);
                    return true;
                }

                op = ResolveOp(
                    makeType(GlobalVar, needsVarInjectionChecks), depth, nullptr, nullptr, entry.watchpointSet(),
                    reinterpret_cast<uintptr_t>(globalObjectScope->variableAt(entry.scopeOffset()).slot()));
                return true;
            }
        }

        PropertySlot slot(globalObjectScope, PropertySlot::InternalMethodType::VMInquiry, &vm);
        bool hasOwnProperty = globalObjectScope->getOwnPropertySlot(globalObjectScope, globalObject, ident, slot);
        slot.disallowVMEntry.reset();
        if (!hasOwnProperty) {
            op = ResolveOp(makeType(UnresolvedProperty, needsVarInjectionChecks), 0, nullptr, nullptr, nullptr, 0);
            return true;
        }

        Structure* structure = globalObjectScope->structure();
        if (!slot.isCacheableValue()
            || !structure->propertyAccessesAreCacheable()
            || (structure->hasReadOnlyOrGetterSetterPropertiesExcludingProto() && getOrPut == Put)) {
            // We know the property will be at global scope, but we don't know how to cache it.
            ASSERT(!globalObjectScope->next());
            op = ResolveOp(makeType(GlobalProperty, needsVarInjectionChecks), 0, nullptr, nullptr, nullptr, 0);
            return true;
        }


        WatchpointState state = structure->ensurePropertyReplacementWatchpointSet(vm, slot.cachedOffset())->state();
        if (state == IsWatched && getOrPut == Put) {
            op = ResolveOp(makeType(GlobalProperty, needsVarInjectionChecks), depth, nullptr, nullptr, nullptr, 0);
        } else
            op = ResolveOp(makeType(GlobalProperty, needsVarInjectionChecks), depth, structure, nullptr, nullptr, slot.cachedOffset());
        return true;
    }

    op = ResolveOp(Dynamic, 0, nullptr, nullptr, nullptr, 0);
    return true;
}

JSObject* objectAtScope(JSObject* scope)
{
    if (scope->type() == WithScopeType)
        return uncheckedDowncast<JSWithScope>(scope)->object();

    return scope;
}

JSObject* nextScope(JSObject* scope)
{
    ASSERT(isScopeChainCell(scope));
WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
    auto* slot = std::bit_cast<WriteBarrier<JSObject>*>(std::bit_cast<char*>(scope) + scopeChainNextOffset);
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
    return slot->get();
}

SymbolTable* scopeSymbolTable(JSObject* scope)
{
    switch (scope->type()) {
    case LexicalEnvironmentType:
    case ModuleEnvironmentType:
        return uncheckedDowncast<JSLexicalEnvironment>(scope)->symbolTable();
    case GlobalLexicalEnvironmentType:
        return uncheckedDowncast<JSGlobalLexicalEnvironment>(scope)->symbolTable();
    case GlobalObjectType:
        return uncheckedDowncast<JSGlobalObject>(scope)->symbolTable();
    default:
        return nullptr;
    }
}

bool isVarScope(JSObject* scope)
{
    if (scope->type() != LexicalEnvironmentType)
        return false;
    return uncheckedDowncast<JSLexicalEnvironment>(scope)->symbolTable()->scopeType() == SymbolTable::ScopeType::VarScope;
}

bool isLexicalScope(JSObject* scope)
{
    if (!scope->isJSLexicalEnvironment())
        return false;
    return uncheckedDowncast<JSLexicalEnvironment>(scope)->symbolTable()->scopeType() == SymbolTable::ScopeType::LexicalScope;
}

bool isModuleScope(JSObject* scope)
{
    return scope->type() == ModuleEnvironmentType;
}

bool isCatchScope(JSObject* scope)
{
    if (scope->type() != LexicalEnvironmentType)
        return false;

    auto scopeType = uncheckedDowncast<JSLexicalEnvironment>(scope)->symbolTable()->scopeType();
    return scopeType == SymbolTable::ScopeType::CatchScope
        || scopeType == SymbolTable::ScopeType::CatchScopeWithSimpleParameter;
}

bool isCatchScopeWithSimpleParameter(JSObject* scope)
{
    if (scope->type() != LexicalEnvironmentType)
        return false;
    return uncheckedDowncast<JSLexicalEnvironment>(scope)->symbolTable()->scopeType() == SymbolTable::ScopeType::CatchScopeWithSimpleParameter;
}

bool isFunctionNameScopeObject(JSObject* scope)
{
    if (scope->type() != LexicalEnvironmentType)
        return false;
    return uncheckedDowncast<JSLexicalEnvironment>(scope)->symbolTable()->scopeType() == SymbolTable::ScopeType::FunctionNameScope;
}

bool isNestedLexicalScope(JSObject* scope)
{
    if (!scope->isJSLexicalEnvironment())
        return false;
    return uncheckedDowncast<JSLexicalEnvironment>(scope)->symbolTable()->isNestedLexicalScope();
}

// When an exception occurs, the result of isUnscopable becomes false.
static inline bool isUnscopable(JSGlobalObject* globalObject, JSObject* scope, JSObject* object, const Identifier& ident)
{
    VM& vm = globalObject->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);
    if (scope->type() != WithScopeType)
        return false;

    JSValue unscopables = object->get(globalObject, vm.propertyNames->unscopablesSymbol);
    RETURN_IF_EXCEPTION(throwScope, false);
    if (!unscopables.isObject())
        return false;
    JSValue blocked = uncheckedDowncast<JSObject>(unscopables)->get(globalObject, ident);
    RETURN_IF_EXCEPTION(throwScope, false);

    return blocked.toBoolean(globalObject);
}

template<typename ReturnPredicateFunctor, typename SkipPredicateFunctor>
ALWAYS_INLINE static JSObject* resolveScopeImpl(JSGlobalObject* globalObject, JSObject* scope, const Identifier& ident, ReturnPredicateFunctor returnPredicate, SkipPredicateFunctor skipPredicate)
{
    VM& vm = globalObject->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);
    ScopeChainIterator end(nullptr);
    ScopeChainIterator it(scope);
    while (1) {
        JSObject* currentScope = it.scope();
        JSObject* object = it.get();

        // Global scope.
        if (++it == end) {
            JSObject* globalScopeExtension = currentScope->realm()->globalScopeExtension();
            if (globalScopeExtension) [[unlikely]] {
                bool hasProperty = object->hasProperty(globalObject, ident);
                RETURN_IF_EXCEPTION(throwScope, nullptr);
                if (hasProperty)
                    return object;
                JSObject* extensionScopeObject = objectAtScope(globalScopeExtension);
                hasProperty = extensionScopeObject->hasProperty(globalObject, ident);
                RETURN_IF_EXCEPTION(throwScope, nullptr);
                if (hasProperty)
                    return extensionScopeObject;
            }
            return object;
        }

        if (skipPredicate(currentScope))
            continue;

        bool hasProperty = object->hasProperty(globalObject, ident);
        RETURN_IF_EXCEPTION(throwScope, nullptr);
        if (hasProperty) {
            bool unscopable = isUnscopable(globalObject, currentScope, object, ident);
            EXCEPTION_ASSERT(!throwScope.exception() || !unscopable);
            if (!unscopable)
                return object;
        }

        if (returnPredicate(currentScope))
            return object;
    }
}

JSValue resolveScopeForHoistingFuncDeclInEval(JSGlobalObject* globalObject, JSObject* scope, const Identifier& ident)
{
    VM& vm = globalObject->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    auto returnPredicate = [&] (JSObject* candidate) -> bool {
        if (isNonGlobalScopeChainCell(candidate))
            return isVarScope(candidate);
        return false;
    };
    auto skipPredicate = [&] (JSObject* candidate) -> bool {
        if (isNonGlobalScopeChainCell(candidate))
            return candidate->isWithScope() || isCatchScopeWithSimpleParameter(candidate);
        return false;
    };
    JSObject* object = resolveScopeImpl(globalObject, scope, ident, returnPredicate, skipPredicate);
    RETURN_IF_EXCEPTION(throwScope, { });

    bool result = false;
    if (object) {
        if (object->isGlobalObject())
            result = true;
        else if (isNonGlobalScopeChainCell(object)) {
            if (SymbolTable* tab = scopeSymbolTable(object))
                result = tab->scopeType() == SymbolTable::ScopeType::VarScope;
        }
    }

    return result ? JSValue(object) : jsUndefined();
}

JSObject* resolveScope(JSGlobalObject* globalObject, JSObject* scope, const Identifier& ident)
{
    auto predicate1 = [&] (JSObject*) -> bool {
        return false;
    };
    auto predicate2 = [&] (JSObject*) -> bool {
        return false;
    };
    return resolveScopeImpl(globalObject, scope, ident, predicate1, predicate2);
}

ResolveOp abstractResolveScope(JSGlobalObject* globalObject, size_t depthOffset, JSObject* scope, const Identifier& ident, GetOrPut getOrPut, ResolveType unlinkedType, InitializationMode initializationMode)
{
    ResolveOp op(Dynamic, 0, nullptr, nullptr, nullptr, 0);
    if (unlinkedType == Dynamic)
        return op;

    bool needsVarInjectionChecks = JSC::needsVarInjectionChecks(unlinkedType);
    size_t depth = depthOffset;
    JSObject* current = scope;
    while (current) {
        bool success = abstractAccess(globalObject, current, ident, getOrPut, depth, needsVarInjectionChecks, op, initializationMode);
        if (success)
            break;
        ++depth;
        if (isNonGlobalScopeChainCell(current))
            current = nextScope(current);
        else
            current = nullptr;
    }

    return op;
}

void collectClosureVariablesUnderTDZ(JSObject* scope, TDZEnvironment& result, PrivateNameEnvironment& privateNameEnvironment)
{
    JSObject* current = scope;
    while (current) {
        if (!isNonGlobalScopeChainCell(current))
            break;

        if (isLexicalScope(current) || isCatchScope(current)) {
            if (isModuleScope(current)) {
                AbstractModuleRecord* moduleRecord = uncheckedDowncast<JSModuleEnvironment>(current)->moduleRecord();
                for (const auto& pair : moduleRecord->importEntries())
                    result.add(pair.key);
            }

            SymbolTable* symbolTable = uncheckedDowncast<JSLexicalEnvironment>(current)->symbolTable();
            ASSERT(symbolTable->scopeType() == SymbolTable::ScopeType::LexicalScope || symbolTable->scopeType() == SymbolTable::ScopeType::CatchScope || symbolTable->scopeType() == SymbolTable::ScopeType::CatchScopeWithSimpleParameter);
            ConcurrentJSLocker locker(symbolTable->m_lock);
            for (auto end = symbolTable->end(locker), iter = symbolTable->begin(locker); iter != end; ++iter)
                result.add(iter->key);

            if (symbolTable->hasPrivateNames()) {
                auto privateNames = symbolTable->privateNames();
                for (auto end = privateNames.end(), iter = privateNames.begin(); iter != end; ++iter)
                    privateNameEnvironment.add(iter->key, iter->value);
            }
        }

        current = nextScope(current);
    }
}

JSObject* constantScopeForCodeBlock(ResolveType type, CodeBlock* codeBlock)
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

bool hasConstantScope(ResolveType type)
{
    switch (type) {
    case GlobalProperty:
    case GlobalVar:
    case GlobalPropertyWithVarInjectionChecks:
    case GlobalVarWithVarInjectionChecks:
    case GlobalLexicalVarWithVarInjectionChecks:
    case GlobalLexicalVar:
        return true;
    default:
        return false;
    }
}

} // namespace JSC
