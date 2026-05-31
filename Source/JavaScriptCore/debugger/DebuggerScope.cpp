/*
 * Copyright (C) 2008-2023 Apple Inc. All rights reserved.
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
#include "DebuggerScope.h"

#include "JSCInlines.h"
#include "JSLexicalEnvironment.h"
#include "StructureCreateInlines.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(DebuggerScope);

const ClassInfo DebuggerScope::s_info = { "DebuggerScope"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(DebuggerScope) };

DebuggerScope* DebuggerScope::create(VM& vm, JSObject* scope)
{
    Structure* structure = scope->realm()->debuggerScopeStructure();
    DebuggerScope* debuggerScope = new (NotNull, allocateCell<DebuggerScope>(vm)) DebuggerScope(vm, structure, scope);
    debuggerScope->finishCreation(vm);
    return debuggerScope;
}

DebuggerScope::DebuggerScope(VM& vm, Structure* structure, JSObject* scope)
    : JSNonFinalObject(vm, structure)
    , m_scope(scope, WriteBarrierEarlyInit)
{
    ASSERT(scope);
}

template<typename Visitor>
void DebuggerScope::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    DebuggerScope* thisObject = uncheckedDowncast<DebuggerScope>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(cell, visitor);

    visitor.append(thisObject->m_scope);
    visitor.append(thisObject->m_next);
}

DEFINE_VISIT_CHILDREN(DebuggerScope);

bool DebuggerScope::getOwnPropertySlot(JSObject* object, JSGlobalObject* globalObject, PropertyName propertyName, PropertySlot& slot)
{
    DebuggerScope* scope = uncheckedDowncast<DebuggerScope>(object);
    if (!scope->isValid())
        return false;
    JSObject* thisObject = objectAtScope(scope->jsScope());
    slot.setThisValue(JSValue(thisObject));

    // By default, JSObject::getPropertySlot() will look in the DebuggerScope's prototype
    // chain and not the wrapped scope, and JSObject::getPropertySlot() cannot be overridden
    // to behave differently for the DebuggerScope.
    //
    // Instead, we'll treat all properties in the wrapped scope and its prototype chain as
    // the own properties of the DebuggerScope. This is fine because the WebInspector
    // does not presently need to distinguish between what's owned at each level in the
    // prototype chain. Hence, we'll invoke getPropertySlot() on the wrapped scope here
    // instead of getOwnPropertySlot().
    bool result = thisObject->getPropertySlot(globalObject, propertyName, slot);
    if (result && slot.isValue() && slot.getValue(globalObject, propertyName) == jsTDZValue()) {
        // FIXME:
        // We hit a scope property that has the TDZ empty value.
        // Currently, we just lie to the inspector and claim that this property is undefined.
        // This is not ideal and we should fix it.
        // https://bugs.webkit.org/show_bug.cgi?id=144977
        slot.setValue(slot.slotBase(), static_cast<unsigned>(PropertyAttribute::DontEnum), jsUndefined());
        return true;
    }
    return result;
}

bool DebuggerScope::put(JSCell* cell, JSGlobalObject* globalObject, PropertyName propertyName, JSValue value, PutPropertySlot& slot)
{
    DebuggerScope* scope = uncheckedDowncast<DebuggerScope>(cell);
    ASSERT(scope->isValid());
    if (!scope->isValid())
        return false;
    JSObject* thisObject = objectAtScope(scope->jsScope());
    slot.setThisValue(JSValue(thisObject));
    return thisObject->methodTable()->put(thisObject, globalObject, propertyName, value, slot);
}

bool DebuggerScope::deleteProperty(JSCell* cell, JSGlobalObject* globalObject, PropertyName propertyName, DeletePropertySlot& slot)
{
    DebuggerScope* scope = uncheckedDowncast<DebuggerScope>(cell);
    ASSERT(scope->isValid());
    if (!scope->isValid())
        return false;
    JSObject* thisObject = objectAtScope(scope->jsScope());
    return thisObject->methodTable()->deleteProperty(thisObject, globalObject, propertyName, slot);
}

void DebuggerScope::getOwnPropertyNames(JSObject* object, JSGlobalObject* globalObject, PropertyNameArrayBuilder& propertyNames, DontEnumPropertiesMode mode)
{
    DebuggerScope* scope = uncheckedDowncast<DebuggerScope>(object);
    ASSERT(scope->isValid());
    if (!scope->isValid())
        return;
    JSObject* thisObject = objectAtScope(scope->jsScope());
    thisObject->getPropertyNames(globalObject, propertyNames, mode);
}

bool DebuggerScope::defineOwnProperty(JSObject* object, JSGlobalObject* globalObject, PropertyName propertyName, const PropertyDescriptor& descriptor, bool shouldThrow)
{
    DebuggerScope* scope = uncheckedDowncast<DebuggerScope>(object);
    ASSERT(scope->isValid());
    if (!scope->isValid())
        return false;
    JSObject* thisObject = objectAtScope(scope->jsScope());
    return thisObject->methodTable()->defineOwnProperty(thisObject, globalObject, propertyName, descriptor, shouldThrow);
}

DebuggerScope* DebuggerScope::next()
{
    ASSERT(isValid());
    if (!m_next) {
        JSObject* current = m_scope.get();
        JSObject* nextLink = nullptr;
        if (current && isNonGlobalScopeChainCell(current))
            nextLink = nextScope(current);
        else if (auto* globalObject = dynamicDowncast<JSGlobalObject>(current))
            nextLink = globalObject->next();
        if (nextLink) {
            VM& vm = current->vm();
            DebuggerScope* nextDebuggerScope = create(vm, nextLink);
            m_next.set(vm, this, nextDebuggerScope);
        }
    }
    return m_next.get();
}

void DebuggerScope::invalidateChain()
{
    if (!isValid())
        return;

    DebuggerScope* scope = this;
    while (scope) {
        DebuggerScope* nextScope = scope->m_next.get();
        scope->m_next.clear();
        scope->m_scope.clear(); // This also marks this scope as invalid.
        scope = nextScope;
    }
}

bool DebuggerScope::isCatchScope() const
{
    JSObject* obj = m_scope.get();
    return obj && isNonGlobalScopeChainCell(obj) && JSC::isCatchScope(obj);
}

bool DebuggerScope::isFunctionNameScope() const
{
    JSObject* obj = m_scope.get();
    return obj && isNonGlobalScopeChainCell(obj) && JSC::isFunctionNameScopeObject(obj);
}

bool DebuggerScope::isWithScope() const
{
    JSObject* obj = m_scope.get();
    return obj && isNonGlobalScopeChainCell(obj) && obj->isWithScope();
}

bool DebuggerScope::isGlobalScope() const
{
    return m_scope->type() == GlobalObjectType;
}

bool DebuggerScope::isGlobalLexicalEnvironment() const
{
    return m_scope->type() == GlobalLexicalEnvironmentType;
}

bool DebuggerScope::isClosureScope() const
{
    JSObject* obj = m_scope.get();
    return obj && isNonGlobalScopeChainCell(obj) && (isVarScope(obj) || isLexicalScope(obj));
}

bool DebuggerScope::isNestedLexicalScope() const
{
    JSObject* obj = m_scope.get();
    return obj && isNonGlobalScopeChainCell(obj) && JSC::isNestedLexicalScope(obj);
}

String DebuggerScope::name() const
{
    SymbolTable* symbolTable = nullptr;
    if (JSObject* obj = m_scope.get())
        symbolTable = scopeSymbolTable(obj);
    if (!symbolTable)
        return String();

    return symbolTable->inferredName();
}

DebuggerLocation DebuggerScope::location() const
{
    SymbolTable* symbolTable = nullptr;
    if (JSObject* obj = m_scope.get())
        symbolTable = scopeSymbolTable(obj);
    if (!symbolTable)
        return DebuggerLocation();

    return symbolTable->debuggerLocation();
}

JSValue DebuggerScope::caughtValue(JSGlobalObject* globalObject) const
{
    ASSERT(isCatchScope());
    JSLexicalEnvironment* catchEnvironment = uncheckedDowncast<JSLexicalEnvironment>(m_scope.get());
    SymbolTable* catchSymbolTable = catchEnvironment->symbolTable();
    RELEASE_ASSERT(catchSymbolTable->size() == 1);
    PropertyName errorName(catchSymbolTable->begin(catchSymbolTable->m_lock)->key.get());
    PropertySlot slot(m_scope.get(), PropertySlot::InternalMethodType::Get);
    bool success = catchEnvironment->getOwnPropertySlot(catchEnvironment, globalObject, errorName, slot);
    RELEASE_ASSERT(success && slot.isValue());
    return slot.getValue(globalObject, errorName);
}

Structure* DebuggerScope::createStructure(VM& vm, JSGlobalObject* globalObject)
{
    return Structure::create(vm, globalObject, jsNull(), TypeInfo(ObjectType, StructureFlags), info());
}

} // namespace JSC
