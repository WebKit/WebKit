/*
 * Copyright (C) 2012, 2013, 2014 Apple Inc. All Rights Reserved.
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

#include "JSActivation.h"
#include "JSGlobalObject.h"
#include "JSNameScope.h"
#include "JSWithScope.h"
#include "JSCInlines.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(JSScope);

void JSScope::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    JSScope* thisObject = jsCast<JSScope*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    COMPILE_ASSERT(StructureFlags & OverridesVisitChildren, OverridesVisitChildrenWithoutSettingFlag);
    ASSERT(thisObject->structure()->typeInfo().overridesVisitChildren());

    Base::visitChildren(thisObject, visitor);
    visitor.append(&thisObject->m_next);
}

// Returns true if we found enough information to terminate optimization.
static inline bool abstractAccess(ExecState* exec, JSScope* scope, const Identifier& ident, GetOrPut getOrPut, size_t depth, bool& needsVarInjectionChecks, ResolveOp& op)
{
    if (JSActivation* activation = jsDynamicCast<JSActivation*>(scope)) {
        if (ident == exec->propertyNames().arguments) {
            // We know the property will be at this activation scope, but we don't know how to cache it.
            op = ResolveOp(Dynamic, 0, 0, 0, 0, 0);
            return true;
        }

        SymbolTableEntry entry = activation->symbolTable()->get(ident.impl());
        if (entry.isReadOnly() && getOrPut == Put) {
            // We know the property will be at this activation scope, but we don't know how to cache it.
            op = ResolveOp(Dynamic, 0, 0, 0, 0, 0);
            return true;
        }

        if (!entry.isNull()) {
            op = ResolveOp(makeType(ClosureVar, needsVarInjectionChecks), depth, 0, activation, entry.watchpointSet(), entry.getIndex());
            return true;
        }

        if (activation->symbolTable()->usesNonStrictEval())
            needsVarInjectionChecks = true;
        return false;
    }

    if (JSGlobalObject* globalObject = jsDynamicCast<JSGlobalObject*>(scope)) {
        SymbolTableEntry entry = globalObject->symbolTable()->get(ident.impl());
        if (!entry.isNull()) {
            if (getOrPut == Put && entry.isReadOnly()) {
                // We know the property will be at global scope, but we don't know how to cache it.
                op = ResolveOp(Dynamic, 0, 0, 0, 0, 0);
                return true;
            }

            op = ResolveOp(
                makeType(GlobalVar, needsVarInjectionChecks), depth, 0, 0, entry.watchpointSet(),
                reinterpret_cast<uintptr_t>(globalObject->registerAt(entry.getIndex()).slot()));
            return true;
        }

        PropertySlot slot(globalObject);
        if (!globalObject->getOwnPropertySlot(globalObject, exec, ident, slot)
            || !slot.isCacheableValue()
            || !globalObject->structure()->propertyAccessesAreCacheable()
            || (globalObject->structure()->hasReadOnlyOrGetterSetterPropertiesExcludingProto() && getOrPut == Put)) {
            // We know the property will be at global scope, but we don't know how to cache it.
            ASSERT(!scope->next());
            op = ResolveOp(makeType(GlobalProperty, needsVarInjectionChecks), depth, 0, 0, 0, 0);
            return true;
        }

        op = ResolveOp(makeType(GlobalProperty, needsVarInjectionChecks), depth, globalObject->structure(), 0, 0, slot.cachedOffset());
        return true;
    }

    op = ResolveOp(Dynamic, 0, 0, 0, 0, 0);
    return true;
}

JSObject* JSScope::objectAtScope(JSScope* scope)
{
    JSObject* object = scope;
    if (object->structure()->typeInfo().type() == WithScopeType)
        return jsCast<JSWithScope*>(object)->object();

    return object;
}

int JSScope::depth()
{
    int depth = 0;
    for (JSScope* scope = this; scope; scope = scope->next())
        ++depth;
    return depth;
}

JSValue JSScope::resolve(ExecState* exec, JSScope* scope, const Identifier& ident)
{
    ScopeChainIterator end = scope->end();
    ScopeChainIterator it = scope->begin();
    while (1) {
        JSObject* object = it.get();

        if (++it == end) // Global scope.
            return object;

        if (object->hasProperty(exec, ident))
            return object;
    }
}

ResolveOp JSScope::abstractResolve(ExecState* exec, JSScope* scope, const Identifier& ident, GetOrPut getOrPut, ResolveType unlinkedType)
{
    ResolveOp op(Dynamic, 0, 0, 0, 0, 0);
    if (unlinkedType == Dynamic)
        return op;

    size_t depth = 0;
    bool needsVarInjectionChecks = JSC::needsVarInjectionChecks(unlinkedType);
    for (; scope; scope = scope->next()) {
        if (abstractAccess(exec, scope, ident, getOrPut, depth, needsVarInjectionChecks, op))
            break;
        ++depth;
    }

    return op;
}

const char* resolveModeName(ResolveMode mode)
{
    static const char* const names[] = {
        "ThrowIfNotFound",
        "DoNotThrowIfNotFound"
    };
    return names[mode];
}

const char* resolveTypeName(ResolveType type)
{
    static const char* const names[] = {
        "GlobalProperty",
        "GlobalVar",
        "ClosureVar",
        "GlobalPropertyWithVarInjectionChecks",
        "GlobalVarWithVarInjectionChecks",
        "ClosureVarWithVarInjectionChecks",
        "Dynamic"
    };
    ASSERT(type < sizeof(names) / sizeof(names[0]));
    return names[type];
}

} // namespace JSC
