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

namespace JSC {

ASSERT_CLASS_FITS_IN_CELL(JSScope);

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

JSValue JSScope::resolve(CallFrame* callFrame, const Identifier& identifier)
{
    ScopeChainNode* scopeChain = callFrame->scopeChain();
    ASSERT(scopeChain);

    do {
        JSObject* scope = scopeChain->object.get();
        PropertySlot slot(scope);
        if (scope->getPropertySlot(callFrame, identifier, slot))
            return slot.getValue(callFrame, identifier);
    } while ((scopeChain = scopeChain->next.get()));

    return throwError(callFrame, createUndefinedVariableError(callFrame, identifier));
}

JSValue JSScope::resolveSkip(CallFrame* callFrame, const Identifier& identifier, int skip)
{
    ScopeChainNode* scopeChain = callFrame->scopeChain();
    ASSERT(scopeChain);

    CodeBlock* codeBlock = callFrame->codeBlock();

    bool checkTopLevel = codeBlock->codeType() == FunctionCode && codeBlock->needsFullScopeChain();
    ASSERT(skip || !checkTopLevel);
    if (checkTopLevel && skip--) {
        if (callFrame->uncheckedR(codeBlock->activationRegister()).jsValue())
            scopeChain = scopeChain->next.get();
    }
    while (skip--) {
        scopeChain = scopeChain->next.get();
        ASSERT(scopeChain);
    }

    do {
        JSObject* scope = scopeChain->object.get();
        PropertySlot slot(scope);
        if (scope->getPropertySlot(callFrame, identifier, slot))
            return slot.getValue(callFrame, identifier);
    } while ((scopeChain = scopeChain->next.get()));

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
    ScopeChainNode* scopeChain = callFrame->scopeChain();
    ASSERT(scopeChain);

    CodeBlock* codeBlock = callFrame->codeBlock();

    bool checkTopLevel = codeBlock->codeType() == FunctionCode && codeBlock->needsFullScopeChain();
    ASSERT(skip || !checkTopLevel);
    if (checkTopLevel && skip--) {
        if (callFrame->uncheckedR(codeBlock->activationRegister()).jsValue())
            scopeChain = scopeChain->next.get();
    }
    while (skip--) {
        JSObject* scope = scopeChain->object.get();
        if (!scope->hasCustomProperties())
            continue;

        PropertySlot slot(scope);
        if (!scope->getPropertySlot(callFrame, identifier, slot))
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
    ScopeChainNode* scopeChain = callFrame->scopeChain();
    ASSERT(scopeChain);

    do {
        JSObject* scope = scopeChain->object.get();

        PropertySlot slot(scope);
        if (!scope->getPropertySlot(callFrame, identifier, slot))
            continue;

        return JSValue(scope);
    } while ((scopeChain = scopeChain->next.get()));

    if (!isStrict)
        return callFrame->lexicalGlobalObject();

    return throwError(callFrame, createErrorForInvalidGlobalAssignment(callFrame, identifier.ustring()));
}

JSValue JSScope::resolveWithBase(CallFrame* callFrame, const Identifier& identifier, Register* base)
{
    ScopeChainNode* scopeChain = callFrame->scopeChain();
    ASSERT(scopeChain);

    do {
        JSObject* scope = scopeChain->object.get();

        PropertySlot slot(scope);
        if (!scope->getPropertySlot(callFrame, identifier, slot))
            continue;

        JSValue value = slot.getValue(callFrame, identifier);
        if (callFrame->globalData().exception)
            return JSValue();

        *base = JSValue(scope);
        return value;
    } while ((scopeChain = scopeChain->next.get()));

    return throwError(callFrame, createUndefinedVariableError(callFrame, identifier));
}

JSValue JSScope::resolveWithThis(CallFrame* callFrame, const Identifier& identifier, Register* base)
{
    ScopeChainNode* scopeChain = callFrame->scopeChain();
    ASSERT(scopeChain);

    do {
        JSObject* scope = scopeChain->object.get();

        PropertySlot slot(scope);
        if (!scope->getPropertySlot(callFrame, identifier, slot))
            continue;

        JSValue value = slot.getValue(callFrame, identifier);
        if (callFrame->globalData().exception)
            return JSValue();

        *base = scope->structure()->typeInfo().isEnvironmentRecord() ? jsUndefined() : JSValue(scope);
        return value;
    } while ((scopeChain = scopeChain->next.get()));

    return throwError(callFrame, createUndefinedVariableError(callFrame, identifier));
}

} // namespace JSC
