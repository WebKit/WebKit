/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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

#include "PropertyNameArray.h"
#include "property_map.h"

namespace KJS {

UString::Rep* IdentifierRepHashTraits::nullRepPtr = &UString::Rep::null; // Didn't want to make a whole source file for just this.

void JSVariableObject::saveSymbolTable(SymbolTable& s) const
{
    s = *d->symbolTable;
}

void JSVariableObject::restoreSymbolTable(SymbolTable& s) const
{
    *d->symbolTable = s;
}

void JSVariableObject::saveLocalStorage(SavedProperties& p) const
{
    unsigned count = d->localStorage.size();

    p.m_properties.clear();
    p.m_count = count;

    if (!count)
        return;

    p.m_properties.set(new SavedProperty[count]);
    
    SavedProperty* prop = p.m_properties.get();
    for (size_t i = 0; i < count; ++i, ++prop) {
        LocalStorageEntry& entry = d->localStorage[i];
        prop->value = entry.value;
        prop->attributes = entry.attributes;
    }
}

void JSVariableObject::restoreLocalStorage(SavedProperties& p) const
{
    unsigned count = p.m_count;
    d->localStorage.resize(count);
    SavedProperty* prop = p.m_properties.get();
    for (size_t i = 0; i < count; ++i, ++prop)
        d->localStorage[i] = LocalStorageEntry(prop->value, prop->attributes);
}

bool JSVariableObject::deleteProperty(ExecState* exec, const Identifier& propertyName)
{
    if (symbolTable().contains(propertyName.ustring().rep()))
        return false;

    return JSObject::deleteProperty(exec, propertyName);
}

void JSVariableObject::getPropertyNames(ExecState* exec, PropertyNameArray& propertyNames)
{
    SymbolTable::const_iterator::Keys end = symbolTable().end().keys();
    for (SymbolTable::const_iterator::Keys it = symbolTable().begin().keys(); it != end; ++it)
        propertyNames.add(Identifier(it->get()));

    JSObject::getPropertyNames(exec, propertyNames);
}

void JSVariableObject::mark()
{
    JSObject::mark();

    size_t size = d->localStorage.size();
    for (size_t i = 0; i < size; ++i) {
        JSValue* value = d->localStorage[i].value;
        if (!value->marked())
            value->mark();
    }
}

} // namespace KJS
