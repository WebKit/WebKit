/*
 * Copyright (C) 2008-2019 Apple Inc. All rights reserved.
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

#include "JSLexicalEnvironment.h"
#include "JSCInlines.h"
#include "JSWithScope.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(DebuggerScope);

const ClassInfo DebuggerScope::s_info = { "DebuggerScope", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(DebuggerScope) };

DebuggerScope* DebuggerScope::create(VM& vm, JSScope* scope)
{
    Structure* structure = scope->globalObject(vm)->debuggerScopeStructure();
    DebuggerScope* debuggerScope = new (NotNull, allocateCell<DebuggerScope>(vm.heap)) DebuggerScope(vm, structure, scope);
    debuggerScope->finishCreation(vm);
    return debuggerScope;
}

DebuggerScope::DebuggerScope(VM& vm, Structure* structure, JSScope* scope)
    : JSNonFinalObject(vm, structure)
{
    ASSERT(scope);
    m_scope.set(vm, this, scope);
}

void DebuggerScope::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
}

void DebuggerScope::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    DebuggerScope* thisObject = jsCast<DebuggerScope*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(cell, visitor);

    visitor.append(thisObject->m_scope);
    visitor.append(thisObject->m_next);
}

String DebuggerScope::className(const JSObject* object, VM& vm)
{
    const DebuggerScope* scope = jsCast<const DebuggerScope*>(object);
    // We cannot assert that scope->isValid() because the TypeProfiler may encounter an invalidated
    // DebuggerScope in its log entries. We just need to handle it appropriately as below.
    if (!scope->isValid())
        return String();
    JSObject* thisObject = JSScope::objectAtScope(scope->jsScope());
    return thisObject->methodTable(vm)->className(thisObject, vm);
}

String DebuggerScope::toStringName(const JSObject* object, JSGlobalObject* globalObject)
{
    const DebuggerScope* scope = jsCast<const DebuggerScope*>(object);
    // We cannot assert that scope->isValid() because the TypeProfiler may encounter an invalidated
    // DebuggerScope in its log entries. We just need to handle it appropriately as below.
    if (!scope->isValid())
        return String();
    JSObject* thisObject = JSScope::objectAtScope(scope->jsScope());
    return thisObject->methodTable(globalObject->vm())->toStringName(thisObject, globalObject);
}

bool DebuggerScope::getOwnPropertySlot(JSObject* object, JSGlobalObject* globalObject, PropertyName propertyName, PropertySlot& slot)
{
    DebuggerScope* scope = jsCast<DebuggerScope*>(object);
    if (!scope->isValid())
        return false;
    JSObject* thisObject = JSScope::objectAtScope(scope->jsScope());
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
    DebuggerScope* scope = jsCast<DebuggerScope*>(cell);
    ASSERT(scope->isValid());
    if (!scope->isValid())
        return false;
    JSObject* thisObject = JSScope::objectAtScope(scope->jsScope());
    slot.setThisValue(JSValue(thisObject));
    return thisObject->methodTable(globalObject->vm())->put(thisObject, globalObject, propertyName, value, slot);
}

bool DebuggerScope::deleteProperty(JSCell* cell, JSGlobalObject* globalObject, PropertyName propertyName)
{
    DebuggerScope* scope = jsCast<DebuggerScope*>(cell);
    ASSERT(scope->isValid());
    if (!scope->isValid())
        return false;
    JSObject* thisObject = JSScope::objectAtScope(scope->jsScope());
    return thisObject->methodTable(globalObject->vm())->deleteProperty(thisObject, globalObject, propertyName);
}

void DebuggerScope::getOwnPropertyNames(JSObject* object, JSGlobalObject* globalObject, PropertyNameArray& propertyNames, EnumerationMode mode)
{
    DebuggerScope* scope = jsCast<DebuggerScope*>(object);
    ASSERT(scope->isValid());
    if (!scope->isValid())
        return;
    JSObject* thisObject = JSScope::objectAtScope(scope->jsScope());
    thisObject->methodTable(globalObject->vm())->getPropertyNames(thisObject, globalObject, propertyNames, mode);
}

bool DebuggerScope::defineOwnProperty(JSObject* object, JSGlobalObject* globalObject, PropertyName propertyName, const PropertyDescriptor& descriptor, bool shouldThrow)
{
    DebuggerScope* scope = jsCast<DebuggerScope*>(object);
    ASSERT(scope->isValid());
    if (!scope->isValid())
        return false;
    JSObject* thisObject = JSScope::objectAtScope(scope->jsScope());
    return thisObject->methodTable(globalObject->vm())->defineOwnProperty(thisObject, globalObject, propertyName, descriptor, shouldThrow);
}

DebuggerScope* DebuggerScope::next()
{
    ASSERT(isValid());
    if (!m_next && m_scope->next()) {
        VM& vm = m_scope->vm();
        DebuggerScope* nextScope = create(vm, m_scope->next());
        m_next.set(vm, this, nextScope);
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
    return m_scope->isCatchScope();
}

bool DebuggerScope::isFunctionNameScope() const
{
    return m_scope->isFunctionNameScopeObject();
}

bool DebuggerScope::isWithScope() const
{
    return m_scope->isWithScope();
}

bool DebuggerScope::isGlobalScope() const
{
    return m_scope->isGlobalObject();
}

bool DebuggerScope::isGlobalLexicalEnvironment() const
{
    return m_scope->isGlobalLexicalEnvironment();
}

bool DebuggerScope::isClosureScope() const
{
    // In the current debugger implementation, every function or eval will create an
    // lexical environment object. Hence, a lexical environment object implies a
    // function or eval scope.
    return m_scope->isVarScope() || m_scope->isLexicalScope();
}

bool DebuggerScope::isNestedLexicalScope() const
{
    return m_scope->isNestedLexicalScope();
}

String DebuggerScope::name() const
{
    SymbolTable* symbolTable = m_scope->symbolTable(vm());
    if (!symbolTable)
        return String();

    CodeBlock* codeBlock = symbolTable->rareDataCodeBlock();
    if (!codeBlock)
        return String();

    return String::fromUTF8(codeBlock->inferredName());
}

DebuggerLocation DebuggerScope::location() const
{
    SymbolTable* symbolTable = m_scope->symbolTable(vm());
    if (!symbolTable)
        return DebuggerLocation();

    CodeBlock* codeBlock = symbolTable->rareDataCodeBlock();
    if (!codeBlock)
        return DebuggerLocation();

    ScriptExecutable* executable = codeBlock->ownerExecutable();
    return DebuggerLocation(executable);
}

JSValue DebuggerScope::caughtValue(JSGlobalObject* globalObject) const
{
    ASSERT(isCatchScope());
    JSLexicalEnvironment* catchEnvironment = jsCast<JSLexicalEnvironment*>(m_scope.get());
    SymbolTable* catchSymbolTable = catchEnvironment->symbolTable();
    RELEASE_ASSERT(catchSymbolTable->size() == 1);
    PropertyName errorName(catchSymbolTable->begin(catchSymbolTable->m_lock)->key.get());
    PropertySlot slot(m_scope.get(), PropertySlot::InternalMethodType::Get);
    bool success = catchEnvironment->getOwnPropertySlot(catchEnvironment, globalObject, errorName, slot);
    RELEASE_ASSERT(success && slot.isValue());
    return slot.getValue(globalObject, errorName);
}

} // namespace JSC
