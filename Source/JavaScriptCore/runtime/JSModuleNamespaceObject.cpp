/*
 * Copyright (C) 2015-2021 Apple Inc. All rights reserved.
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

#include "AbstractModuleRecord.h"
#include "CyclicModuleRecord.h"
#include "JSCInlines.h"
#include "JSModuleEnvironment.h"

namespace JSC {

const ClassInfo JSModuleNamespaceObject::s_info = { "ModuleNamespaceObject"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSModuleNamespaceObject) };

JSModuleNamespaceObject::JSModuleNamespaceObject(VM& vm, Structure* structure, AbstractModuleRecord* moduleRecord, Vector<std::pair<Identifier, AbstractModuleRecord::Resolution>>&& resolutions, bool isDeferred)
    : Base(vm, structure)
    , m_exports()
    , m_moduleRecord(moduleRecord, WriteBarrierEarlyInit)
    , m_isDeferred(isDeferred)
{
    // http://www.ecma-international.org/ecma-262/6.0/#sec-module-namespace-exotic-objects
    // Quoted from the spec:
    //     A List containing the String values of the exported names exposed as own properties of this object.
    //     The list is ordered as if an Array of those String values had been sorted using Array.prototype.sort using SortCompare as comparator.
    //
    // Sort the exported names by the code point order.
    std::ranges::sort(resolutions, WTF::codePointCompareLessThan, [](const auto& resolution) {
        return StringView(resolution.first.impl());
    });

    for (const auto& pair : resolutions) {
        m_exports.add(pair.first.impl(), ExportEntry {
            pair.second.localName,
            WriteBarrier { pair.second.moduleRecord, WriteBarrierEarlyInit },
        });
    }
}

void JSModuleNamespaceObject::finishCreation(JSGlobalObject* globalObject)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
    putDirect(vm, vm.propertyNames->toStringTagSymbol, jsNontrivialString(vm, m_isDeferred ? "Deferred Module"_s : "Module"_s), PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);

    // http://www.ecma-international.org/ecma-262/6.0/#sec-module-namespace-exotic-objects-getprototypeof
    // http://www.ecma-international.org/ecma-262/6.0/#sec-module-namespace-exotic-objects-setprototypeof-v
    // http://www.ecma-international.org/ecma-262/6.0/#sec-module-namespace-exotic-objects-isextensible
    // http://www.ecma-international.org/ecma-262/6.0/#sec-module-namespace-exotic-objects-preventextensions
    scope.release();
    methodTable()->preventExtensions(this, globalObject);
}

void JSModuleNamespaceObject::destroy(JSCell* cell)
{
    JSModuleNamespaceObject* thisObject = static_cast<JSModuleNamespaceObject*>(cell);
    thisObject->JSModuleNamespaceObject::~JSModuleNamespaceObject();
}

template<typename Visitor>
void JSModuleNamespaceObject::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    JSModuleNamespaceObject* thisObject = uncheckedDowncast<JSModuleNamespaceObject>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
    visitor.append(thisObject->m_moduleRecord);
    for (auto& entry : thisObject->m_exports.values())
        visitor.appendHidden(entry.moduleRecord);
}

DEFINE_VISIT_CHILDREN(JSModuleNamespaceObject);

// https://tc39.es/proposal-defer-import-eval/#sec-IsSymbolLikeNamespaceKey
ALWAYS_INLINE bool JSModuleNamespaceObject::isSymbolLikeNamespaceKey(VM& vm, PropertyName propertyName)
{
    return propertyName.isSymbol() || (m_isDeferred && propertyName == vm.propertyNames->then);
}

// https://tc39.es/proposal-defer-import-eval/#sec-GetModuleExportsList
// The spec returns O.[[Exports]] after triggering evaluation; here we only perform the
// evaluation side effect since callers already have direct access to m_exports.
void JSModuleNamespaceObject::ensureDeferredNamespaceEvaluation(JSGlobalObject* globalObject)
{
    // 1. If O.[[Deferred]] is true, then
    ASSERT(m_isDeferred);
    // Fast path: if the module's cycle has already successfully evaluated, EvaluateModuleSync would
    // observe a fulfilled promise and return without throwing, so we can skip the work entirely.
    // We must consult [[CycleRoot]] here because Evaluate() redirects to it; for a non-root SCC
    // member, status/evaluationError on the module itself may not reflect the cycle's outcome.
    if (auto* cyclic = dynamicDowncast<CyclicModuleRecord>(m_moduleRecord.get())) {
        CyclicModuleRecord* root = cyclic->cycleRoot() ? cyclic->cycleRoot() : cyclic;
        if (root->status() == CyclicModuleRecord::Status::Evaluated && !root->evaluationError())
            return;
    }
    //   1.a. Let m be O.[[Module]].
    //   1.b. Perform ? EvaluateModuleSync(m).
    m_moduleRecord->evaluateSync(globalObject);
    // 2. Return O.[[Exports]].
}

static JSValue getValue(JSModuleEnvironment* environment, PropertyName localName, ScopeOffset& scopeOffset)
{
    SymbolTable* symbolTable = environment->symbolTable();
    {
        ConcurrentJSLocker locker(symbolTable->m_lock);
        auto iter = symbolTable->find(locker, localName.uid());
        ASSERT(iter != symbolTable->end(locker));
        SymbolTableEntry& entry = iter->value;
        ASSERT(!entry.isNull());
        scopeOffset = entry.scopeOffset();
    }
    return environment->variableAt(scopeOffset).get();
}

bool JSModuleNamespaceObject::getOwnPropertySlotCommon(JSGlobalObject* globalObject, PropertyName propertyName, PropertySlot& slot)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // http://www.ecma-international.org/ecma-262/6.0/#sec-module-namespace-exotic-objects-getownproperty-p

    // 1. If IsSymbolLikeNamespaceKey(P, O) is true, then return ! Ordinary{Get,GetOwnProperty,HasProperty}(O, P, ...).
    // It may return the descriptor with writable: true, but namespace objects does not allow it in [[Set]] / [[DefineOwnProperty]] side.
    if (isSymbolLikeNamespaceKey(vm, propertyName))
        return Base::getOwnPropertySlot(this, globalObject, propertyName, slot);

    slot.setIsTaintedByOpaqueObject();

    if (m_isDeferred && slot.internalMethodType() != PropertySlot::InternalMethodType::VMInquiry) [[unlikely]] {
        ensureDeferredNamespaceEvaluation(globalObject);
        RETURN_IF_EXCEPTION(scope, false);
    }

    auto iterator = m_exports.find(propertyName.uid());
    if (iterator == m_exports.end())
        return false;
    ExportEntry& exportEntry = iterator->value;

    switch (slot.internalMethodType()) {
    case PropertySlot::InternalMethodType::GetOwnProperty:
    case PropertySlot::InternalMethodType::Get: {
        if (exportEntry.localName == vm.propertyNames->starNamespacePrivateName) {
            // https://tc39.es/ecma262/#sec-module-namespace-exotic-objects-get-p-receiver
            // 10. If binding.[[BindingName]] is "*namespace*", then
            //     a. Return ? GetModuleNamespace(targetModule).
            // We call getModuleNamespace() to ensure materialization. And after that, looking up the value from the scope to encourage module namespace object IC.
            exportEntry.moduleRecord->getModuleNamespace(globalObject);
            RETURN_IF_EXCEPTION(scope, false);
        }
        JSModuleEnvironment* environment = exportEntry.moduleRecord->moduleEnvironment();
        ScopeOffset scopeOffset;
        JSValue value = getValue(environment, exportEntry.localName, scopeOffset);
        // If the value is filled with TDZ value, throw a reference error.
        if (!value) {
            RefPtr uid = propertyName.uid();
            throwVMError(globalObject, scope, createTDZError(globalObject, *uid));
            return false;
        }

        slot.setValueModuleNamespace(this, static_cast<unsigned>(PropertyAttribute::DontDelete), value, environment, scopeOffset);
        return true;
    }

    case PropertySlot::InternalMethodType::HasProperty: {
        // Do not perform [[Get]] for [[HasProperty]].
        // [[Get]] / [[GetOwnProperty]] onto namespace object could throw an error while [[HasProperty]] just returns true here.
        // https://tc39.github.io/ecma262/#sec-module-namespace-exotic-objects-hasproperty-p
        slot.setValue(this, static_cast<unsigned>(PropertyAttribute::DontDelete), jsUndefined());
        return true;
    }

    case PropertySlot::InternalMethodType::VMInquiry:
        slot.setValue(this, static_cast<unsigned>(JSC::PropertyAttribute::None), jsUndefined());
        return false;
    }

    RELEASE_ASSERT_NOT_REACHED();
    return false;
}

bool JSModuleNamespaceObject::getOwnPropertySlot(JSObject* cell, JSGlobalObject* globalObject, PropertyName propertyName, PropertySlot& slot)
{
    JSModuleNamespaceObject* thisObject = uncheckedDowncast<JSModuleNamespaceObject>(cell);
    return thisObject->getOwnPropertySlotCommon(globalObject, propertyName, slot);
}

bool JSModuleNamespaceObject::getOwnPropertySlotByIndex(JSObject* cell, JSGlobalObject* globalObject, unsigned propertyName, PropertySlot& slot)
{
    VM& vm = globalObject->vm();
    JSModuleNamespaceObject* thisObject = uncheckedDowncast<JSModuleNamespaceObject>(cell);
    return thisObject->getOwnPropertySlotCommon(globalObject, Identifier::from(vm, propertyName), slot);
}

bool JSModuleNamespaceObject::put(JSCell*, JSGlobalObject* globalObject, PropertyName, JSValue, PutPropertySlot& slot)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // http://www.ecma-international.org/ecma-262/6.0/#sec-module-namespace-exotic-objects-set-p-v-receiver
    if (slot.isStrictMode())
        throwTypeError(globalObject, scope, ReadonlyPropertyWriteError);
    return false;
}

bool JSModuleNamespaceObject::putByIndex(JSCell*, JSGlobalObject* globalObject, unsigned, JSValue, bool shouldThrow)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (shouldThrow)
        throwTypeError(globalObject, scope, ReadonlyPropertyWriteError);
    return false;
}

bool JSModuleNamespaceObject::deleteProperty(JSCell* cell, JSGlobalObject* globalObject, PropertyName propertyName, DeletePropertySlot& slot)
{
    // https://tc39.es/ecma262/#sec-module-namespace-exotic-objects-delete-p
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSModuleNamespaceObject* thisObject = uncheckedDowncast<JSModuleNamespaceObject>(cell);
    if (thisObject->isSymbolLikeNamespaceKey(vm, propertyName))
        RELEASE_AND_RETURN(scope, Base::deleteProperty(thisObject, globalObject, propertyName, slot));

    if (thisObject->m_isDeferred) [[unlikely]] {
        thisObject->ensureDeferredNamespaceEvaluation(globalObject);
        RETURN_IF_EXCEPTION(scope, false);
    }

    return !thisObject->m_exports.contains(propertyName.uid());
}

bool JSModuleNamespaceObject::deletePropertyByIndex(JSCell* cell, JSGlobalObject* globalObject, unsigned propertyName)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSModuleNamespaceObject* thisObject = uncheckedDowncast<JSModuleNamespaceObject>(cell);
    if (thisObject->m_isDeferred) [[unlikely]] {
        thisObject->ensureDeferredNamespaceEvaluation(globalObject);
        RETURN_IF_EXCEPTION(scope, false);
    }
    return !thisObject->m_exports.contains(Identifier::from(vm, propertyName).impl());
}

void JSModuleNamespaceObject::getOwnPropertyNames(JSObject* cell, JSGlobalObject* globalObject, PropertyNameArrayBuilder& propertyNames, DontEnumPropertiesMode mode)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // https://tc39.es/ecma262/#sec-module-namespace-exotic-objects-ownpropertykeys
    JSModuleNamespaceObject* thisObject = uncheckedDowncast<JSModuleNamespaceObject>(cell);
    if (thisObject->m_isDeferred) [[unlikely]] {
        thisObject->ensureDeferredNamespaceEvaluation(globalObject);
        RETURN_IF_EXCEPTION(scope, void());
    }
    for (const auto& name : thisObject->m_exports.keys()) {
        if (mode == DontEnumPropertiesMode::Exclude) {
            // Perform [[GetOwnProperty]] to throw ReferenceError if binding is uninitialized.
            PropertySlot slot(cell, PropertySlot::InternalMethodType::GetOwnProperty);
            bool hasProperty = thisObject->getOwnPropertySlotCommon(globalObject, name.get(), slot);
            RETURN_IF_EXCEPTION(scope, void());
            if (!hasProperty)
                continue;
        }
        propertyNames.add(name);
    }
    if (propertyNames.includeSymbolProperties()) {
        scope.release();
        thisObject->getOwnNonIndexPropertyNames(globalObject, propertyNames, mode);
    }
}

bool JSModuleNamespaceObject::defineOwnProperty(JSObject* cell, JSGlobalObject* globalObject, PropertyName propertyName, const PropertyDescriptor& descriptor, bool shouldThrow)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // https://tc39.es/ecma262/#sec-module-namespace-exotic-objects-defineownproperty-p-desc

    JSModuleNamespaceObject* thisObject = uncheckedDowncast<JSModuleNamespaceObject>(cell);

    // 1. If IsSymbolLikeNamespaceKey(P, O) is true, return ! OrdinaryDefineOwnProperty(O, P, Desc).
    if (thisObject->isSymbolLikeNamespaceKey(vm, propertyName))
        RELEASE_AND_RETURN(scope, Base::defineOwnProperty(thisObject, globalObject, propertyName, descriptor, shouldThrow));

    // 2. Let current be ? O.[[GetOwnProperty]](P).
    PropertyDescriptor current;
    bool isCurrentDefined = thisObject->getOwnPropertyDescriptor(globalObject, propertyName, current);
    RETURN_IF_EXCEPTION(scope, false);

    // 3. If current is undefined, return false.
    if (!isCurrentDefined) {
        if (shouldThrow)
            throwTypeError(globalObject, scope, NonExtensibleObjectPropertyDefineError);
        return false;
    }

    // 4. If IsAccessorDescriptor(Desc) is true, return false.
    if (descriptor.isAccessorDescriptor()) {
        if (shouldThrow)
            throwTypeError(globalObject, scope, "Cannot change module namespace object's binding to accessor"_s);
        return false;
    }

    // 5. If Desc.[[Writable]] is present and has value false, return false.
    if (descriptor.writablePresent() && !descriptor.writable()) {
        if (shouldThrow)
            throwTypeError(globalObject, scope, "Cannot change module namespace object's binding to non-writable attribute"_s);
        return false;
    }

    // 6. If Desc.[[Enumerable]] is present and has value false, return false.
    if (descriptor.enumerablePresent() && !descriptor.enumerable()) {
        if (shouldThrow)
            throwTypeError(globalObject, scope, "Cannot replace module namespace object's binding with non-enumerable attribute"_s);
        return false;
    }

    // 7. If Desc.[[Configurable]] is present and has value true, return false.
    if (descriptor.configurablePresent() && descriptor.configurable()) {
        if (shouldThrow)
            throwTypeError(globalObject, scope, "Cannot replace module namespace object's binding with configurable attribute"_s);
        return false;
    }

    // 8. If Desc.[[Value]] is present, return SameValue(Desc.[[Value]], current.[[Value]]).
    if (descriptor.value()) {
        bool result = sameValue(globalObject, descriptor.value(), current.value());
        RETURN_IF_EXCEPTION(scope, false);
        if (!result) {
            if (shouldThrow)
                throwTypeError(globalObject, scope, "Cannot replace module namespace object's binding's value"_s);
            return false;
        }
    }

    // 9. Return true.
    return true;
}

} // namespace JSC
