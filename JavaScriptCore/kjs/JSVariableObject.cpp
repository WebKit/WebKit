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

#include "PropertyNameArray.h"
#include "property_map.h"

namespace KJS {

UString::Rep* IdentifierRepHashTraits::nullRepPtr = &UString::Rep::null; // Didn't want to make a whole source file for just this.

void JSVariableObject::saveLocalStorage(SavedProperties& p) const
{
    ASSERT(d->symbolTable);
    ASSERT(static_cast<size_t>(d->symbolTable->size()) == d->localStorage.size());

    unsigned count = d->symbolTable->size();

    p.properties.clear();
    p.count = count;

    if (!count)
        return;

    p.properties.set(new SavedProperty[count]);

    SymbolTable::const_iterator end = d->symbolTable->end();
    for (SymbolTable::const_iterator it = d->symbolTable->begin(); it != end; ++it) {
        size_t i = it->second;
        const LocalStorageEntry& entry = d->localStorage[i];
        p.properties[i].init(it->first.get(), entry.value, entry.attributes);
    }
}

void JSVariableObject::restoreLocalStorage(const SavedProperties& p)
{
    unsigned count = p.count;
    d->symbolTable->clear();
    d->localStorage.resize(count);
    SavedProperty* property = p.properties.get();
    for (size_t i = 0; i < count; ++i, ++property) {
        ASSERT(!d->symbolTable->contains(property->name()));
        LocalStorageEntry& entry = d->localStorage[i];
        d->symbolTable->set(property->name(), i);
        entry.value = property->value();
        entry.attributes = property->attributes();
    }
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
