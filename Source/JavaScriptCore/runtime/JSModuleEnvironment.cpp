/*
 * Copyright (C) 2015-2019 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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
#include "JSModuleEnvironment.h"

#include "AbstractModuleRecord.h"
#include "JSCInlines.h"

namespace JSC {

const ClassInfo JSModuleEnvironment::s_info = { "JSModuleEnvironment", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSModuleEnvironment) };

JSModuleEnvironment* JSModuleEnvironment::create(
    VM& vm, Structure* structure, JSScope* currentScope, SymbolTable* symbolTable, JSValue initialValue, AbstractModuleRecord* moduleRecord)
{
    // JSLexicalEnvironment has the storage to store the variable slots after the its class storage.
    // Because the offset of the variable slots are fixed in the JSLexicalEnvironment, inheritting these class and adding new member field is not allowed,
    // the new member will overlap the variable slots.
    // To keep the JSModuleEnvironment compatible to the JSLexicalEnvironment but add the new member to store the AbstractModuleRecord, we additionally allocate
    // the storage after the variable slots.
    //
    // JSLexicalEnvironment:
    //     [ JSLexicalEnvironment ][ variable slots ]
    //
    // JSModuleEnvironment:
    //     [ JSLexicalEnvironment ][ variable slots ][ additional slots for JSModuleEnvironment ]
    JSModuleEnvironment* result =
        new (
            NotNull,
            allocateCell<JSModuleEnvironment>(vm.heap, JSModuleEnvironment::allocationSize(symbolTable)))
        JSModuleEnvironment(vm, structure, currentScope, symbolTable);
    result->finishCreation(vm, initialValue, moduleRecord);
    return result;
}

void JSModuleEnvironment::finishCreation(VM& vm, JSValue initialValue, AbstractModuleRecord* moduleRecord)
{
    Base::finishCreation(vm, initialValue);
    this->moduleRecordSlot().set(vm, this, moduleRecord);
}

void JSModuleEnvironment::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    JSModuleEnvironment* thisObject = jsCast<JSModuleEnvironment*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
    visitor.appendValues(thisObject->variables(), thisObject->symbolTable()->scopeSize());
    visitor.append(thisObject->moduleRecordSlot());
}

bool JSModuleEnvironment::getOwnPropertySlot(JSObject* cell, JSGlobalObject* globalObject, PropertyName propertyName, PropertySlot& slot)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSModuleEnvironment* thisObject = jsCast<JSModuleEnvironment*>(cell);
    AbstractModuleRecord::Resolution resolution = thisObject->moduleRecord()->resolveImport(globalObject, Identifier::fromUid(vm, propertyName.uid()));
    RETURN_IF_EXCEPTION(scope, false);
    if (resolution.type == AbstractModuleRecord::Resolution::Type::Resolved) {
        // When resolveImport resolves the resolution, the imported module environment must have the binding.
        JSModuleEnvironment* importedModuleEnvironment = resolution.moduleRecord->moduleEnvironment();
        PropertySlot redirectSlot(importedModuleEnvironment, PropertySlot::InternalMethodType::Get);
        bool result = importedModuleEnvironment->methodTable(vm)->getOwnPropertySlot(importedModuleEnvironment, globalObject, resolution.localName, redirectSlot);
        ASSERT_UNUSED(result, result);
        ASSERT(redirectSlot.isValue());
        JSValue value = redirectSlot.getValue(globalObject, resolution.localName);
        scope.assertNoException();
        slot.setValue(thisObject, redirectSlot.attributes(), value);
        return true;
    }
    return Base::getOwnPropertySlot(thisObject, globalObject, propertyName, slot);
}

void JSModuleEnvironment::getOwnNonIndexPropertyNames(JSObject* cell, JSGlobalObject* globalObject, PropertyNameArray& propertyNamesArray, EnumerationMode mode)
{
    JSModuleEnvironment* thisObject = jsCast<JSModuleEnvironment*>(cell);
    if (propertyNamesArray.includeStringProperties()) {
        for (const auto& pair : thisObject->moduleRecord()->importEntries()) {
            const AbstractModuleRecord::ImportEntry& importEntry = pair.value;
            if (importEntry.type == AbstractModuleRecord::ImportEntryType::Single)
                propertyNamesArray.add(importEntry.localName);
        }
    }
    return Base::getOwnNonIndexPropertyNames(thisObject, globalObject, propertyNamesArray, mode);
}

bool JSModuleEnvironment::put(JSCell* cell, JSGlobalObject* globalObject, PropertyName propertyName, JSValue value, PutPropertySlot& slot)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSModuleEnvironment* thisObject = jsCast<JSModuleEnvironment*>(cell);
    // All imported bindings are immutable.
    AbstractModuleRecord::Resolution resolution = thisObject->moduleRecord()->resolveImport(globalObject, Identifier::fromUid(vm, propertyName.uid()));
    RETURN_IF_EXCEPTION(scope, false);
    if (resolution.type == AbstractModuleRecord::Resolution::Type::Resolved) {
        throwTypeError(globalObject, scope, ReadonlyPropertyWriteError);
        return false;
    }
    RELEASE_AND_RETURN(scope, Base::put(thisObject, globalObject, propertyName, value, slot));
}

bool JSModuleEnvironment::deleteProperty(JSCell* cell, JSGlobalObject* globalObject, PropertyName propertyName, DeletePropertySlot& slot)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSModuleEnvironment* thisObject = jsCast<JSModuleEnvironment*>(cell);
    // All imported bindings are immutable.
    AbstractModuleRecord::Resolution resolution = thisObject->moduleRecord()->resolveImport(globalObject, Identifier::fromUid(vm, propertyName.uid()));
    RETURN_IF_EXCEPTION(scope, false);
    if (resolution.type == AbstractModuleRecord::Resolution::Type::Resolved)
        return false;
    return Base::deleteProperty(thisObject, globalObject, propertyName, slot);
}

} // namespace JSC
