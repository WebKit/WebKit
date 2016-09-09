/*
 * Copyright (C) 2015-2016 Apple Inc. All rights reserved.
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
#include "JSCInlines.h"
#include "JSModuleEnvironment.h"
#include "JSModuleRecord.h"
#include "JSPropertyNameIterator.h"

namespace JSC {

static EncodedJSValue JSC_HOST_CALL moduleNamespaceObjectSymbolIterator(ExecState*);

const ClassInfo JSModuleNamespaceObject::s_info = { "ModuleNamespaceObject", &Base::s_info, nullptr, CREATE_METHOD_TABLE(JSModuleNamespaceObject) };


JSModuleNamespaceObject::JSModuleNamespaceObject(VM& vm, Structure* structure)
    : Base(vm, structure)
    , m_exports()
{
}

void JSModuleNamespaceObject::finishCreation(ExecState* exec, JSGlobalObject* globalObject, JSModuleRecord* moduleRecord, const IdentifierSet& exports)
{
    VM& vm = exec->vm();
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
    JSFunction* iteratorFunction = JSFunction::create(vm, globalObject, 0, ASCIILiteral("[Symbol.iterator]"), moduleNamespaceObjectSymbolIterator, NoIntrinsic);
    putDirect(vm, vm.propertyNames->iteratorSymbol, iteratorFunction, DontEnum);
    putDirect(vm, vm.propertyNames->toStringTagSymbol, jsString(&vm, "Module"), DontEnum | ReadOnly);

    // http://www.ecma-international.org/ecma-262/6.0/#sec-module-namespace-exotic-objects-getprototypeof
    // http://www.ecma-international.org/ecma-262/6.0/#sec-module-namespace-exotic-objects-setprototypeof-v
    // http://www.ecma-international.org/ecma-262/6.0/#sec-module-namespace-exotic-objects-isextensible
    // http://www.ecma-international.org/ecma-262/6.0/#sec-module-namespace-exotic-objects-preventextensions
    methodTable(vm)->preventExtensions(this, exec);
    ASSERT(!exec->hadException());
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

bool JSModuleNamespaceObject::getOwnPropertySlot(JSObject* cell, ExecState* exec, PropertyName propertyName, PropertySlot& slot)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // http://www.ecma-international.org/ecma-262/6.0/#sec-module-namespace-exotic-objects-getownproperty-p

    JSModuleNamespaceObject* thisObject = jsCast<JSModuleNamespaceObject*>(cell);

    // step 1.
    // If the property name is a symbol, we don't look into the imported bindings.
    // It may return the descriptor with writable: true, but namespace objects does not allow it in [[Set]] / [[DefineOwnProperty]] side.
    if (propertyName.isSymbol())
        return JSObject::getOwnPropertySlot(thisObject, exec, propertyName, slot);

    // FIXME: Add IC for module namespace object.
    // https://bugs.webkit.org/show_bug.cgi?id=160590
    slot.disableCaching();
    slot.setIsTaintedByOpaqueObject();
    if (!thisObject->m_exports.contains(propertyName.uid()))
        return false;

    switch (slot.internalMethodType()) {
    case PropertySlot::InternalMethodType::Get:
    case PropertySlot::InternalMethodType::GetOwnProperty: {
        JSModuleRecord* moduleRecord = thisObject->moduleRecord();

        JSModuleRecord::Resolution resolution = moduleRecord->resolveExport(exec, Identifier::fromUid(exec, propertyName.uid()));
        ASSERT(resolution.type != JSModuleRecord::Resolution::Type::NotFound && resolution.type != JSModuleRecord::Resolution::Type::Ambiguous);

        JSModuleRecord* targetModule = resolution.moduleRecord;
        JSModuleEnvironment* targetEnvironment = targetModule->moduleEnvironment();

        PropertySlot trampolineSlot(targetEnvironment, PropertySlot::InternalMethodType::Get);
        bool found = targetEnvironment->methodTable(vm)->getOwnPropertySlot(targetEnvironment, exec, resolution.localName, trampolineSlot);
        ASSERT_UNUSED(found, found);

        JSValue value = trampolineSlot.getValue(exec, propertyName);
        ASSERT(!exec->hadException());

        // If the value is filled with TDZ value, throw a reference error.
        if (!value) {
            throwVMError(exec, scope, createTDZError(exec));
            return false;
        }

        slot.setValue(thisObject, DontDelete, value);
        return true;
    }
    case PropertySlot::InternalMethodType::HasProperty: {
        // Do not perform [[Get]] for [[HasProperty]].
        // [[Get]] / [[GetOwnProperty]] onto namespace object could throw an error while [[HasProperty]] just returns true here.
        // https://tc39.github.io/ecma262/#sec-module-namespace-exotic-objects-hasproperty-p
        slot.setValue(thisObject, DontDelete, jsUndefined());
        return true;
    }

    case PropertySlot::InternalMethodType::VMInquiry:
        return false;
    }

    RELEASE_ASSERT_NOT_REACHED();
    return false;
}

bool JSModuleNamespaceObject::put(JSCell*, ExecState* exec, PropertyName, JSValue, PutPropertySlot& slot)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // http://www.ecma-international.org/ecma-262/6.0/#sec-module-namespace-exotic-objects-set-p-v-receiver
    if (slot.isStrictMode())
        throwTypeError(exec, scope, ASCIILiteral(StrictModeReadonlyPropertyWriteError));
    return false;
}

bool JSModuleNamespaceObject::putByIndex(JSCell*, ExecState* exec, unsigned, JSValue, bool shouldThrow)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (shouldThrow)
        throwTypeError(exec, scope, ASCIILiteral(StrictModeReadonlyPropertyWriteError));
    return false;
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
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // http://www.ecma-international.org/ecma-262/6.0/#sec-module-namespace-exotic-objects-defineownproperty-p-desc
    if (shouldThrow)
        throwTypeError(exec, scope, ASCIILiteral("Attempting to define property on object that is not extensible."));
    return false;
}

EncodedJSValue JSC_HOST_CALL moduleNamespaceObjectSymbolIterator(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSModuleNamespaceObject* object = jsDynamicCast<JSModuleNamespaceObject*>(exec->thisValue());
    if (!object)
        return throwVMTypeError(exec, scope, ASCIILiteral("|this| should be a module namespace object"));
    return JSValue::encode(JSPropertyNameIterator::create(exec, exec->lexicalGlobalObject()->propertyNameIteratorStructure(), object));
}

} // namespace JSC
