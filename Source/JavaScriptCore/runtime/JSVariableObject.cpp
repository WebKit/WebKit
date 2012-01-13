/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "JSVariableObject.h"

#include "JSActivation.h"
#include "JSGlobalObject.h"
#include "JSStaticScopeObject.h"
#include "PropertyNameArray.h"
#include "PropertyDescriptor.h"

namespace JSC {

void JSVariableObject::destroy(JSCell* cell)
{
    jsCast<JSVariableObject*>(cell)->JSVariableObject::~JSVariableObject();
}

bool JSVariableObject::deleteProperty(JSCell* cell, ExecState* exec, const Identifier& propertyName)
{
    JSVariableObject* thisObject = jsCast<JSVariableObject*>(cell);
    if (thisObject->symbolTable().contains(propertyName.impl()))
        return false;

    return JSObject::deleteProperty(thisObject, exec, propertyName);
}

void JSVariableObject::getOwnPropertyNames(JSObject* object, ExecState* exec, PropertyNameArray& propertyNames, EnumerationMode mode)
{
    JSVariableObject* thisObject = jsCast<JSVariableObject*>(object);
    SymbolTable::const_iterator end = thisObject->symbolTable().end();
    for (SymbolTable::const_iterator it = thisObject->symbolTable().begin(); it != end; ++it) {
        if (!(it->second.getAttributes() & DontEnum) || (mode == IncludeDontEnumProperties))
            propertyNames.add(Identifier(exec, it->first.get()));
    }
    
    JSObject::getOwnPropertyNames(thisObject, exec, propertyNames, mode);
}

bool JSVariableObject::symbolTableGet(const Identifier& propertyName, PropertyDescriptor& descriptor)
{
    SymbolTableEntry entry = symbolTable().inlineGet(propertyName.impl());
    if (!entry.isNull()) {
        descriptor.setDescriptor(registerAt(entry.getIndex()).get(), entry.getAttributes() | DontDelete);
        return true;
    }
    return false;
}

void JSVariableObject::putDirectVirtual(JSObject*, ExecState*, const Identifier&, JSValue, unsigned)
{
    ASSERT_NOT_REACHED();
}

bool JSVariableObject::isDynamicScope(bool& requiresDynamicChecks) const
{
    switch (structure()->typeInfo().type()) {
    case GlobalObjectType:
        return static_cast<const JSGlobalObject*>(this)->isDynamicScope(requiresDynamicChecks);
    case ActivationObjectType:
        return static_cast<const JSActivation*>(this)->isDynamicScope(requiresDynamicChecks);
    case StaticScopeObjectType:
        return static_cast<const JSStaticScopeObject*>(this)->isDynamicScope(requiresDynamicChecks);
    default:
        ASSERT_NOT_REACHED();
        break;
    }

    return false;
}

} // namespace JSC
