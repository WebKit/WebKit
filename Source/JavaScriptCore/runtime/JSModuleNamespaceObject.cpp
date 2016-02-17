/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "JSModuleNamespaceObject.h"

#include "Error.h"
#include "IdentifierInlines.h"
#include "JSCJSValueInlines.h"
#include "JSCellInlines.h"
#include "JSModuleEnvironment.h"
#include "JSModuleRecord.h"
#include "JSPropertyNameIterator.h"
#include "SlotVisitorInlines.h"
#include "StructureInlines.h"

namespace JSC {

static EncodedJSValue JSC_HOST_CALL moduleNamespaceObjectSymbolIterator(ExecState*);

const ClassInfo JSModuleNamespaceObject::s_info = { "ModuleNamespaceObject", &Base::s_info, nullptr, CREATE_METHOD_TABLE(JSModuleNamespaceObject) };


JSModuleNamespaceObject::JSModuleNamespaceObject(VM& vm, Structure* structure)
    : Base(vm, structure)
    , m_exports()
{
}

void JSModuleNamespaceObject::finishCreation(VM& vm, JSGlobalObject* globalObject, JSModuleRecord* moduleRecord, const IdentifierSet& exports)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));

    // http://www.ecma-international.org/ecma-262/6.0/#sec-module-namespace-exotic-objects
    // Quoted from the spec:
    //     A List containing the String values of the exported names exposed as own properties of this object.
    //     The list is ordered as if an Array of those String values had been sorted using Array.prototype.sort using SortCompare as comparefn.
    //
    // Sort the exported names by the code point order.
    Vector<UniquedStringImpl*> temporaryVector(exports.size(), nullptr);
    std::transform(exports.begin(), exports.end(), temporaryVector.begin(), [](const RefPtr<WTF::UniquedStringImpl>& ref) {
        return ref.get();
    });
    std::sort(temporaryVector.begin(), temporaryVector.end(), [] (UniquedStringImpl* lhs, UniquedStringImpl* rhs) {
        return codePointCompare(lhs, rhs) < 0;
    });
    for (auto* identifier : temporaryVector)
        m_exports.add(identifier);

    m_moduleRecord.set(vm, this, moduleRecord);
    JSC_NATIVE_FUNCTION(vm.propertyNames->iteratorSymbol, moduleNamespaceObjectSymbolIterator, DontEnum, 0);
    putDirect(vm, vm.propertyNames->toStringTagSymbol, jsString(&vm, "Module"), DontEnum | ReadOnly);

    // http://www.ecma-international.org/ecma-262/6.0/#sec-module-namespace-exotic-objects-getprototypeof
    // http://www.ecma-international.org/ecma-262/6.0/#sec-module-namespace-exotic-objects-setprototypeof-v
    // http://www.ecma-international.org/ecma-262/6.0/#sec-module-namespace-exotic-objects-isextensible
    // http://www.ecma-international.org/ecma-262/6.0/#sec-module-namespace-exotic-objects-preventextensions
    preventExtensions(vm);
}

void JSModuleNamespaceObject::destroy(JSCell* cell)
{
    JSModuleNamespaceObject* thisObject = jsCast<JSModuleNamespaceObject*>(cell);
    thisObject->JSModuleNamespaceObject::~JSModuleNamespaceObject();
}

void JSModuleNamespaceObject::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    JSModuleNamespaceObject* thisObject = jsCast<JSModuleNamespaceObject*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
    visitor.append(&thisObject->m_moduleRecord);
}

static EncodedJSValue callbackGetter(ExecState* exec, EncodedJSValue thisValue, PropertyName propertyName)
{
    JSModuleNamespaceObject* thisObject = jsCast<JSModuleNamespaceObject*>(JSValue::decode(thisValue));
    JSModuleRecord* moduleRecord = thisObject->moduleRecord();

    JSModuleRecord::Resolution resolution = moduleRecord->resolveExport(exec, Identifier::fromUid(exec, propertyName.uid()));
    ASSERT(resolution.type != JSModuleRecord::Resolution::Type::NotFound && resolution.type != JSModuleRecord::Resolution::Type::Ambiguous);

    JSModuleRecord* targetModule = resolution.moduleRecord;
    JSModuleEnvironment* targetEnvironment = targetModule->moduleEnvironment();

    PropertySlot trampolineSlot(targetEnvironment, PropertySlot::InternalMethodType::Get);
    if (!targetEnvironment->methodTable(exec->vm())->getOwnPropertySlot(targetEnvironment, exec, resolution.localName, trampolineSlot))
        return JSValue::encode(jsUndefined());

    JSValue value = trampolineSlot.getValue(exec, propertyName);
    if (exec->hadException())
        return JSValue::encode(jsUndefined());

    // If the value is filled with TDZ value, throw a reference error.
    if (!value)
        return throwVMError(exec, createTDZError(exec));
    return JSValue::encode(value);
}

bool JSModuleNamespaceObject::getOwnPropertySlot(JSObject* cell, ExecState* exec, PropertyName propertyName, PropertySlot& slot)
{
    // http://www.ecma-international.org/ecma-262/6.0/#sec-module-namespace-exotic-objects-getownproperty-p

    JSModuleNamespaceObject* thisObject = jsCast<JSModuleNamespaceObject*>(cell);

    // step 1.
    // If the property name is a symbol, we don't look into the imported bindings.
    // It may return the descriptor with writable: true, but namespace objects does not allow it in [[Set]] / [[DefineOwnProperty]] side.
    if (propertyName.isSymbol())
        return JSObject::getOwnPropertySlot(thisObject, exec, propertyName, slot);

    if (!thisObject->m_exports.contains(propertyName.uid()))
        return false;

    // https://esdiscuss.org/topic/march-24-meeting-notes
    // http://www.ecma-international.org/ecma-262/6.0/#sec-module-namespace-exotic-objects-getownproperty-p
    // section 9.4.6.5, step 6.
    // This property will be seen as writable: true, enumerable:true, configurable: false.
    // But this does not mean that this property is writable by users.
    //
    // In JSC, getOwnPropertySlot is not designed to throw any errors. But looking up the value from the module
    // environment may throw error if the loaded variable is the TDZ value. To workaround, we set the custom
    // getter function. When it is called, it looks up the variable and throws an error if the variable is not
    // initialized.
    slot.setCustom(thisObject, DontDelete, callbackGetter);
    return true;
}

void JSModuleNamespaceObject::put(JSCell*, ExecState* exec, PropertyName, JSValue, PutPropertySlot& slot)
{
    // http://www.ecma-international.org/ecma-262/6.0/#sec-module-namespace-exotic-objects-set-p-v-receiver
    if (slot.isStrictMode())
        throwTypeError(exec, ASCIILiteral(StrictModeReadonlyPropertyWriteError));
}

void JSModuleNamespaceObject::putByIndex(JSCell*, ExecState* exec, unsigned, JSValue, bool shouldThrow)
{
    if (shouldThrow)
        throwTypeError(exec, ASCIILiteral(StrictModeReadonlyPropertyWriteError));
}

bool JSModuleNamespaceObject::deleteProperty(JSCell* cell, ExecState*, PropertyName propertyName)
{
    // http://www.ecma-international.org/ecma-262/6.0/#sec-module-namespace-exotic-objects-delete-p
    JSModuleNamespaceObject* thisObject = jsCast<JSModuleNamespaceObject*>(cell);
    return !thisObject->m_exports.contains(propertyName.uid());
}

void JSModuleNamespaceObject::getOwnPropertyNames(JSObject* cell, ExecState* exec, PropertyNameArray& propertyNames, EnumerationMode mode)
{
    // http://www.ecma-international.org/ecma-262/6.0/#sec-module-namespace-exotic-objects-ownpropertykeys
    JSModuleNamespaceObject* thisObject = jsCast<JSModuleNamespaceObject*>(cell);
    for (const auto& name : thisObject->m_exports)
        propertyNames.add(name.get());
    return JSObject::getOwnPropertyNames(thisObject, exec, propertyNames, mode);
}

bool JSModuleNamespaceObject::defineOwnProperty(JSObject*, ExecState* exec, PropertyName, const PropertyDescriptor&, bool shouldThrow)
{
    // http://www.ecma-international.org/ecma-262/6.0/#sec-module-namespace-exotic-objects-defineownproperty-p-desc
    if (shouldThrow)
        throwTypeError(exec, ASCIILiteral("Attempting to define property on object that is not extensible."));
    return false;
}

EncodedJSValue JSC_HOST_CALL moduleNamespaceObjectSymbolIterator(ExecState* exec)
{
    JSValue thisValue = exec->thisValue();
    if (!thisValue.isObject())
        return JSValue::encode(throwTypeError(exec, ASCIILiteral("|this| should be an object")));
    return JSValue::encode(JSPropertyNameIterator::create(exec, exec->lexicalGlobalObject()->propertyNameIteratorStructure(), asObject(thisValue)));
}

} // namespace JSC
