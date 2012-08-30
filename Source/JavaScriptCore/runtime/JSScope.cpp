/*
 * Copyright (C) 2012 Apple Inc. All Rights Reserved.
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

namespace JSC {

ASSERT_CLASS_FITS_IN_CELL(JSScope);

void JSScope::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    JSScope* thisObject = jsCast<JSScope*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, &s_info);
    COMPILE_ASSERT(StructureFlags & OverridesVisitChildren, OverridesVisitChildrenWithoutSettingFlag);
    ASSERT(thisObject->structure()->typeInfo().overridesVisitChildren());

    Base::visitChildren(thisObject, visitor);
    visitor.append(&thisObject->m_next);
    visitor.append(&thisObject->m_globalObject);
    visitor.append(&thisObject->m_globalThis);
}

bool JSScope::isDynamicScope(bool& requiresDynamicChecks) const
{
    switch (structure()->typeInfo().type()) {
    case GlobalObjectType:
        return static_cast<const JSGlobalObject*>(this)->isDynamicScope(requiresDynamicChecks);
    case ActivationObjectType:
        return static_cast<const JSActivation*>(this)->isDynamicScope(requiresDynamicChecks);
    case NameScopeObjectType:
        return static_cast<const JSNameScope*>(this)->isDynamicScope(requiresDynamicChecks);
    default:
        ASSERT_NOT_REACHED();
        break;
    }

    return false;
}

JSObject* JSScope::objectAtScope(JSScope* scope)
{
    JSObject* object = scope;
    if (object->structure()->typeInfo().type() == WithScopeType)
        return jsCast<JSWithScope*>(object)->object();

    return object;
}

int JSScope::localDepth()
{
    int scopeDepth = 0;
    ScopeChainIterator iter = this->begin();
    ScopeChainIterator end = this->end();
    while (!iter->inherits(&JSActivation::s_info)) {
        ++iter;
        if (iter == end)
            break;
        ++scopeDepth;
    }
    return scopeDepth;
}

JSValue JSScope::resolve(CallFrame* callFrame, const Identifier& identifier)
{
    JSScope* scope = callFrame->scope();
    ASSERT(scope);

    do {
        JSObject* object = JSScope::objectAtScope(scope);
        PropertySlot slot(object);
        if (object->getPropertySlot(callFrame, identifier, slot))
            return slot.getValue(callFrame, identifier);
    } while ((scope = scope->next()));

    return throwError(callFrame, createUndefinedVariableError(callFrame, identifier));
}

JSValue JSScope::resolveSkip(CallFrame* callFrame, const Identifier& identifier, int skip)
{
    JSScope* scope = callFrame->scope();
    ASSERT(scope);

    CodeBlock* codeBlock = callFrame->codeBlock();

    bool checkTopLevel = codeBlock->codeType() == FunctionCode && codeBlock->needsFullScopeChain();
    ASSERT(skip || !checkTopLevel);
    if (checkTopLevel && skip--) {
        if (callFrame->uncheckedR(codeBlock->activationRegister()).jsValue())
            scope = scope->next();
    }
    while (skip--) {
        scope = scope->next();
        ASSERT(scope);
    }

    do {
        JSObject* object = JSScope::objectAtScope(scope);
        PropertySlot slot(object);
        if (object->getPropertySlot(callFrame, identifier, slot))
            return slot.getValue(callFrame, identifier);
    } while ((scope = scope->next()));

    return throwError(callFrame, createUndefinedVariableError(callFrame, identifier));
}

JSValue JSScope::resolveGlobal(
    CallFrame* callFrame,
    const Identifier& identifier,
    JSGlobalObject* globalObject,
    WriteBarrierBase<Structure>* cachedStructure,
    PropertyOffset* cachedOffset
)
{
    if (globalObject->structure() == cachedStructure->get())
        return globalObject->getDirectOffset(*cachedOffset);

    PropertySlot slot(globalObject);
    if (!globalObject->getPropertySlot(callFrame, identifier, slot))
        return throwError(callFrame, createUndefinedVariableError(callFrame, identifier));

    JSValue result = slot.getValue(callFrame, identifier);
    if (callFrame->globalData().exception)
        return JSValue();

    if (slot.isCacheableValue() && !globalObject->structure()->isUncacheableDictionary() && slot.slotBase() == globalObject) {
        cachedStructure->set(callFrame->globalData(), callFrame->codeBlock()->ownerExecutable(), globalObject->structure());
        *cachedOffset = slot.cachedOffset();
    }
    return result;
}

JSValue JSScope::resolveGlobalDynamic(
    CallFrame* callFrame,
    const Identifier& identifier,
    int skip,
    WriteBarrierBase<Structure>* cachedStructure,
    PropertyOffset* cachedOffset
)
{
    JSScope* scope = callFrame->scope();
    ASSERT(scope);

    CodeBlock* codeBlock = callFrame->codeBlock();

    bool checkTopLevel = codeBlock->codeType() == FunctionCode && codeBlock->needsFullScopeChain();
    ASSERT(skip || !checkTopLevel);
    if (checkTopLevel && skip--) {
        if (callFrame->uncheckedR(codeBlock->activationRegister()).jsValue())
            scope = scope->next();
    }
    while (skip--) {
        JSObject* object = JSScope::objectAtScope(scope);
        if (!object->hasCustomProperties())
            continue;

        PropertySlot slot(object);
        if (!object->getPropertySlot(callFrame, identifier, slot))
            continue;

        JSValue result = slot.getValue(callFrame, identifier);
        if (callFrame->globalData().exception)
            return JSValue();
        return result;
    }

    return resolveGlobal(callFrame, identifier, callFrame->lexicalGlobalObject(), cachedStructure, cachedOffset);
}

JSValue JSScope::resolveBase(CallFrame* callFrame, const Identifier& identifier, bool isStrict)
{
    JSScope* scope = callFrame->scope();
    ASSERT(scope);

    do {
        JSObject* object = JSScope::objectAtScope(scope);

        PropertySlot slot(object);
        if (!object->getPropertySlot(callFrame, identifier, slot))
            continue;

        return JSValue(object);
    } while ((scope = scope->next()));

    if (!isStrict)
        return callFrame->lexicalGlobalObject();

    return throwError(callFrame, createErrorForInvalidGlobalAssignment(callFrame, identifier.ustring()));
}

JSValue JSScope::resolveWithBase(CallFrame* callFrame, const Identifier& identifier, Register* base)
{
    JSScope* scope = callFrame->scope();
    ASSERT(scope);

    do {
        JSObject* object = JSScope::objectAtScope(scope);

        PropertySlot slot(object);
        if (!object->getPropertySlot(callFrame, identifier, slot))
            continue;

        JSValue value = slot.getValue(callFrame, identifier);
        if (callFrame->globalData().exception)
            return JSValue();

        *base = JSValue(object);
        return value;
    } while ((scope = scope->next()));

    return throwError(callFrame, createUndefinedVariableError(callFrame, identifier));
}

JSValue JSScope::resolveWithThis(CallFrame* callFrame, const Identifier& identifier, Register* base)
{
    JSScope* scope = callFrame->scope();
    ASSERT(scope);

    do {
        JSObject* object = JSScope::objectAtScope(scope);

        PropertySlot slot(object);
        if (!object->getPropertySlot(callFrame, identifier, slot))
            continue;

        JSValue value = slot.getValue(callFrame, identifier);
        if (callFrame->globalData().exception)
            return JSValue();

        *base = object->structure()->typeInfo().isEnvironmentRecord() ? jsUndefined() : JSValue(object);
        return value;
    } while ((scope = scope->next()));

    return throwError(callFrame, createUndefinedVariableError(callFrame, identifier));
}

} // namespace JSC
