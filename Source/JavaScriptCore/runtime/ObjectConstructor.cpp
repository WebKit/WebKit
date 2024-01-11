/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2008-2022 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "config.h"
#include "ObjectConstructor.h"

#include "BuiltinNames.h"
#include "JSArray.h"
#include "JSCInlines.h"
#include "JSImmutableButterfly.h"
#include "ObjectConstructorInlines.h"
#include "PropertyDescriptor.h"
#include "PropertyNameArray.h"
#include "Symbol.h"

namespace JSC {

static JSC_DECLARE_HOST_FUNCTION(objectConstructorAssign);
static JSC_DECLARE_HOST_FUNCTION(objectConstructorEntries);
static JSC_DECLARE_HOST_FUNCTION(objectConstructorValues);
static JSC_DECLARE_HOST_FUNCTION(objectConstructorGetPrototypeOf);
static JSC_DECLARE_HOST_FUNCTION(objectConstructorSetPrototypeOf);
static JSC_DECLARE_HOST_FUNCTION(objectConstructorDefineProperty);
static JSC_DECLARE_HOST_FUNCTION(objectConstructorDefineProperties);
static JSC_DECLARE_HOST_FUNCTION(objectConstructorCreate);
static JSC_DECLARE_HOST_FUNCTION(objectConstructorSeal);
static JSC_DECLARE_HOST_FUNCTION(objectConstructorFreeze);
static JSC_DECLARE_HOST_FUNCTION(objectConstructorPreventExtensions);
static JSC_DECLARE_HOST_FUNCTION(objectConstructorIsSealed);
static JSC_DECLARE_HOST_FUNCTION(objectConstructorIsFrozen);
static JSC_DECLARE_HOST_FUNCTION(objectConstructorIsExtensible);
static JSC_DECLARE_HOST_FUNCTION(objectConstructorHasOwn);

}

#include "ObjectConstructor.lut.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(ObjectConstructor);

const ClassInfo ObjectConstructor::s_info = { "Function"_s, &InternalFunction::s_info, &objectConstructorTable, nullptr, CREATE_METHOD_TABLE(ObjectConstructor) };

/* Source for ObjectConstructor.lut.h
@begin objectConstructorTable
  getPrototypeOf            objectConstructorGetPrototypeOf             DontEnum|Function 1 ObjectGetPrototypeOfIntrinsic
  setPrototypeOf            objectConstructorSetPrototypeOf             DontEnum|Function 2
  getOwnPropertyDescriptor  objectConstructorGetOwnPropertyDescriptor   DontEnum|Function 2
  getOwnPropertyDescriptors objectConstructorGetOwnPropertyDescriptors  DontEnum|Function 1
  getOwnPropertyNames       objectConstructorGetOwnPropertyNames        DontEnum|Function 1 ObjectGetOwnPropertyNamesIntrinsic
  getOwnPropertySymbols     objectConstructorGetOwnPropertySymbols      DontEnum|Function 1 ObjectGetOwnPropertySymbolsIntrinsic
  keys                      objectConstructorKeys                       DontEnum|Function 1 ObjectKeysIntrinsic
  defineProperty            objectConstructorDefineProperty             DontEnum|Function 3
  defineProperties          objectConstructorDefineProperties           DontEnum|Function 2
  create                    objectConstructorCreate                     DontEnum|Function 2 ObjectCreateIntrinsic
  seal                      objectConstructorSeal                       DontEnum|Function 1
  freeze                    objectConstructorFreeze                     DontEnum|Function 1
  preventExtensions         objectConstructorPreventExtensions          DontEnum|Function 1
  isSealed                  objectConstructorIsSealed                   DontEnum|Function 1
  isFrozen                  objectConstructorIsFrozen                   DontEnum|Function 1
  isExtensible              objectConstructorIsExtensible               DontEnum|Function 1
  is                        objectConstructorIs                         DontEnum|Function 2 ObjectIsIntrinsic
  assign                    objectConstructorAssign                     DontEnum|Function 2 ObjectAssignIntrinsic
  values                    objectConstructorValues                     DontEnum|Function 1
  entries                   objectConstructorEntries                    DontEnum|Function 1
  fromEntries               JSBuiltin                                   DontEnum|Function 1
@end
*/


static JSC_DECLARE_HOST_FUNCTION(callObjectConstructor);
static JSC_DECLARE_HOST_FUNCTION(constructWithObjectConstructor);

ObjectConstructor::ObjectConstructor(VM& vm, Structure* structure)
    : InternalFunction(vm, structure, callObjectConstructor, constructWithObjectConstructor)
{
}

void ObjectConstructor::finishCreation(VM& vm, JSGlobalObject* globalObject, ObjectPrototype* objectPrototype)
{
    Base::finishCreation(vm, 1, vm.propertyNames->Object.string(), PropertyAdditionMode::WithoutStructureTransition);

    putDirectWithoutTransition(vm, vm.propertyNames->prototype, objectPrototype, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);

    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().getPrototypeOfPrivateName(), objectConstructorGetPrototypeOf, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().getOwnPropertyDescriptorPrivateName(), objectConstructorGetOwnPropertyDescriptor, static_cast<unsigned>(PropertyAttribute::DontEnum), 2, ImplementationVisibility::Public);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().getOwnPropertyNamesPrivateName(), objectConstructorGetOwnPropertyNames, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().getOwnPropertySymbolsPrivateName(), objectConstructorGetOwnPropertySymbols, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().keysPrivateName(), objectConstructorKeys, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().definePropertyPrivateName(), objectConstructorDefineProperty, static_cast<unsigned>(PropertyAttribute::DontEnum), 3, ImplementationVisibility::Public);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().createPrivateName(), objectConstructorCreate, static_cast<unsigned>(PropertyAttribute::DontEnum), 2, ImplementationVisibility::Public);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().valuesPrivateName(), objectConstructorValues, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->hasOwn, objectConstructorHasOwn, static_cast<unsigned>(PropertyAttribute::DontEnum), 2, ImplementationVisibility::Public);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().hasOwnPrivateName(), objectConstructorHasOwn, static_cast<unsigned>(PropertyAttribute::DontEnum), 2, ImplementationVisibility::Public);

    if (Options::useArrayGroupMethod())
        JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().groupByPublicName(), objectConstructorGroupByCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
}

// ES 19.1.1.1 Object([value])
static ALWAYS_INLINE JSObject* constructObjectWithNewTarget(JSGlobalObject* globalObject, CallFrame* callFrame, JSValue newTarget)
{
    VM& vm = globalObject->vm();
    ObjectConstructor* objectConstructor = jsCast<ObjectConstructor*>(callFrame->jsCallee());
    auto scope = DECLARE_THROW_SCOPE(vm);

    // We need to check newTarget condition in this caller side instead of InternalFunction::createSubclassStructure side.
    // Since if we found this condition is met, we should not fall into the type conversion in the step 3.

    // 1. If NewTarget is neither undefined nor the active function, then
    if (newTarget && newTarget != objectConstructor) {
        // a. Return ? OrdinaryCreateFromConstructor(NewTarget, "%ObjectPrototype%").
        JSGlobalObject* functionGlobalObject = getFunctionRealm(globalObject, asObject(newTarget));
        RETURN_IF_EXCEPTION(scope, nullptr);
        Structure* baseStructure = functionGlobalObject->objectStructureForObjectConstructor();
        Structure* objectStructure = InternalFunction::createSubclassStructure(globalObject, asObject(newTarget), baseStructure);
        RETURN_IF_EXCEPTION(scope, nullptr);
        return constructEmptyObject(vm, objectStructure);
    }

    // 2. If value is null, undefined or not supplied, return ObjectCreate(%ObjectPrototype%).
    JSValue argument = callFrame->argument(0);
    if (argument.isUndefinedOrNull())
        return constructEmptyObject(vm, globalObject->objectStructureForObjectConstructor());

    // 3. Return ToObject(value).
    RELEASE_AND_RETURN(scope, argument.toObject(globalObject));
}

JSC_DEFINE_HOST_FUNCTION(constructWithObjectConstructor, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return JSValue::encode(constructObjectWithNewTarget(globalObject, callFrame, callFrame->newTarget()));
}

JSC_DEFINE_HOST_FUNCTION(callObjectConstructor, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return JSValue::encode(constructObjectWithNewTarget(globalObject, callFrame, JSValue()));
}

JSC_DEFINE_HOST_FUNCTION(objectConstructorGetPrototypeOf, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return JSValue::encode(callFrame->argument(0).getPrototype(globalObject));
}

JSC_DEFINE_HOST_FUNCTION(objectConstructorSetPrototypeOf, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue objectValue = callFrame->argument(0);
    if (objectValue.isUndefinedOrNull())
        return throwVMTypeError(globalObject, scope, "Cannot set prototype of undefined or null"_s);

    JSValue protoValue = callFrame->argument(1);
    if (!protoValue.isObject() && !protoValue.isNull())
        return throwVMTypeError(globalObject, scope, PrototypeValueCanOnlyBeAnObjectOrNullTypeError);

    JSObject* object = objectValue.toObject(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    bool shouldThrowIfCantSet = true;
    bool didSetPrototype = object->setPrototype(vm, globalObject, protoValue, shouldThrowIfCantSet);
    EXCEPTION_ASSERT_UNUSED(didSetPrototype, scope.exception() || didSetPrototype);
    return JSValue::encode(objectValue);
}

JSValue objectConstructorGetOwnPropertyDescriptor(JSGlobalObject* globalObject, JSObject* object, const Identifier& propertyName)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    PropertyDescriptor descriptor;
    if (!object->getOwnPropertyDescriptor(globalObject, propertyName, descriptor))
        RELEASE_AND_RETURN(scope, jsUndefined());
    RETURN_IF_EXCEPTION(scope, { });

    JSObject* result = constructObjectFromPropertyDescriptor(globalObject, descriptor);
    scope.assertNoException();
    ASSERT(result);
    return result;
}

JSValue objectConstructorGetOwnPropertyDescriptors(JSGlobalObject* globalObject, JSObject* object)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    PropertyNameArray properties(vm, PropertyNameMode::StringsAndSymbols, PrivateSymbolMode::Exclude);
    object->methodTable()->getOwnPropertyNames(object, globalObject, properties, DontEnumPropertiesMode::Include);
    RETURN_IF_EXCEPTION(scope, { });

    JSObject* descriptors = constructEmptyObject(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    for (auto& propertyName : properties) {
        PropertyDescriptor descriptor;
        bool didGetDescriptor = object->getOwnPropertyDescriptor(globalObject, propertyName, descriptor);
        RETURN_IF_EXCEPTION(scope, { });

        if (!didGetDescriptor)
            continue;

        JSObject* fromDescriptor = constructObjectFromPropertyDescriptor(globalObject, descriptor);
        scope.assertNoException();
        ASSERT(fromDescriptor);

        PutPropertySlot slot(descriptors);
        descriptors->putOwnDataPropertyMayBeIndex(globalObject, propertyName, fromDescriptor, slot);
        scope.assertNoExceptionExceptTermination();
        RETURN_IF_EXCEPTION(scope, { });
    }

    return descriptors;
}

JSC_DEFINE_HOST_FUNCTION(objectConstructorGetOwnPropertyDescriptor, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSObject* object = callFrame->argument(0).toObject(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    auto propertyName = callFrame->argument(1).toPropertyKey(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    RELEASE_AND_RETURN(scope, JSValue::encode(objectConstructorGetOwnPropertyDescriptor(globalObject, object, propertyName)));
}

JSC_DEFINE_HOST_FUNCTION(objectConstructorGetOwnPropertyDescriptors, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSObject* object = callFrame->argument(0).toObject(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    RELEASE_AND_RETURN(scope, JSValue::encode(objectConstructorGetOwnPropertyDescriptors(globalObject, object)));
}

JSC_DEFINE_HOST_FUNCTION(objectConstructorGetOwnPropertyNames, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSObject* object = callFrame->argument(0).toObject(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    RELEASE_AND_RETURN(scope, JSValue::encode(ownPropertyKeys(globalObject, object, PropertyNameMode::Strings, DontEnumPropertiesMode::Include)));
}

JSC_DEFINE_HOST_FUNCTION(objectConstructorGetOwnPropertySymbols, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSObject* object = callFrame->argument(0).toObject(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    RELEASE_AND_RETURN(scope, JSValue::encode(ownPropertyKeys(globalObject, object, PropertyNameMode::Symbols, DontEnumPropertiesMode::Include)));
}

JSC_DEFINE_HOST_FUNCTION(objectConstructorKeys, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSObject* object = callFrame->argument(0).toObject(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    RELEASE_AND_RETURN(scope, JSValue::encode(ownPropertyKeys(globalObject, object, PropertyNameMode::Strings, DontEnumPropertiesMode::Exclude)));
}

void objectAssignGeneric(JSGlobalObject* globalObject, VM& vm, JSObject* target, JSObject* source)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    // [[GetOwnPropertyNames]], [[Get]] etc. could modify target object and invalidate this assumption.
    // For example, [[Get]] of source object could configure setter to target object. So disable the fast path.

    PropertyNameArray properties(vm, PropertyNameMode::StringsAndSymbols, PrivateSymbolMode::Exclude);
    source->methodTable()->getOwnPropertyNames(source, globalObject, properties, DontEnumPropertiesMode::Include);
    RETURN_IF_EXCEPTION(scope, void());

    unsigned numProperties = properties.size();
    for (unsigned j = 0; j < numProperties; j++) {
        const auto& propertyName = properties[j];
        ASSERT(!propertyName.isPrivateName());

        PropertySlot slot(source, PropertySlot::InternalMethodType::GetOwnProperty);
        bool hasProperty = source->methodTable()->getOwnPropertySlot(source, globalObject, propertyName, slot);
        RETURN_IF_EXCEPTION(scope, void());
        if (!hasProperty)
            continue;
        if (slot.attributes() & PropertyAttribute::DontEnum)
            continue;

        JSValue value;
        if (LIKELY(!slot.isTaintedByOpaqueObject()))
            value = slot.getValue(globalObject, propertyName);
        else
            value = source->get(globalObject, propertyName);
        RETURN_IF_EXCEPTION(scope, void());

        PutPropertySlot putPropertySlot(target, true);
        target->putInline(globalObject, propertyName, value, putPropertySlot);
        RETURN_IF_EXCEPTION(scope, void());
    }
}

JSC_DEFINE_HOST_FUNCTION(objectConstructorAssign, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue targetValue = callFrame->argument(0);
    if (targetValue.isUndefinedOrNull())
        return throwVMTypeError(globalObject, scope, "Object.assign requires that input parameter not be null or undefined"_s);
    JSObject* target = targetValue.toObject(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    // FIXME: Extend this for non JSFinalObject. For example, we would like to use this fast path for function objects too.
    // https://bugs.webkit.org/show_bug.cgi?id=185358
    JSFinalObject* targetObject = jsDynamicCast<JSFinalObject*>(target);
    bool targetCanPerformFastPut = targetObject && targetObject->canPerformFastPutInlineExcludingProto() && targetObject->isStructureExtensible();
    unsigned argsCount = callFrame->argumentCount();

    // argsCount == 2 case does not need to use arguments' batching.
    // We limit argsCount < 5 not to increase properties / values vector super large.
    if (argsCount > 2 && argsCount < 5 && targetCanPerformFastPut) {
        bool willBatch = true;
        for (unsigned i = 1; i < argsCount; ++i) {
            JSValue sourceValue = callFrame->uncheckedArgument(i);
            if (!sourceValue.isObject()) {
                willBatch = false;
                break;
            }
            JSObject* source = asObject(sourceValue);
            if (!source->staticPropertiesReified() || !source->structure()->canPerformFastPropertyEnumerationCommon() || source->canHaveExistingOwnIndexedProperties()) {
                willBatch = false;
                break;
            }
        }
        if (willBatch) {
            Vector<RefPtr<UniquedStringImpl>, 16> properties;
            MarkedArgumentBufferWithSize<16> values;
            for (unsigned i = 1; i < argsCount; ++i) {
                JSValue sourceValue = callFrame->uncheckedArgument(i);
                JSObject* source = asObject(sourceValue);
                source->structure()->forEachProperty(vm, [&](const PropertyTableEntry& entry) -> bool {
                    if (entry.attributes() & PropertyAttribute::DontEnum)
                        return true;

                    PropertyName propertyName(entry.key());
                    if (propertyName.isPrivateName())
                        return true;

                    properties.append(entry.key());
                    values.appendWithCrashOnOverflow(source->getDirect(entry.offset()));

                    return true;
                });
            }

            // Actually, assigning with empty object (option for example) is common. (`Object.assign(defaultOptions, passedOptions)` where `passedOptions` is empty object.)
            if (properties.size())
                target->putOwnDataPropertyBatching(vm, properties.data(), values.data(), properties.size());
            return JSValue::encode(target);
        }
    }

    Vector<RefPtr<UniquedStringImpl>, 8> properties;
    MarkedArgumentBuffer values;
    for (unsigned i = 1; i < argsCount; ++i) {
        JSValue sourceValue = callFrame->uncheckedArgument(i);
        if (sourceValue.isUndefinedOrNull())
            continue;
        JSObject* source = sourceValue.toObject(globalObject);
        RETURN_IF_EXCEPTION(scope, { });

        if (targetCanPerformFastPut) {
            if (!source->staticPropertiesReified()) {
                source->reifyAllStaticProperties(globalObject);
                RETURN_IF_EXCEPTION(scope, { });
            }

            bool objectAssignFastSucceeded = objectAssignFast(globalObject, targetObject, source, properties, values);
            RETURN_IF_EXCEPTION(scope, { });
            if (objectAssignFastSucceeded)
                continue;
        }

        targetCanPerformFastPut = false;
        objectAssignGeneric(globalObject, vm, target, source);
        RETURN_IF_EXCEPTION(scope, { });
    }
    return JSValue::encode(target);
}

JSC_DEFINE_HOST_FUNCTION(objectConstructorEntries, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue targetValue = callFrame->argument(0);
    if (targetValue.isUndefinedOrNull())
        return throwVMTypeError(globalObject, scope, "Object.entries requires that input parameter not be null or undefined"_s);
    JSObject* target = targetValue.toObject(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    if (!target->staticPropertiesReified()) {
        target->reifyAllStaticProperties(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
    }

    {
        Vector<RefPtr<UniquedStringImpl>, 8> properties;
        MarkedArgumentBuffer values;
        bool canUseFastPath = false;
        if (!target->canHaveExistingOwnIndexedProperties() && !target->hasNonReifiedStaticProperties()) {
            Structure* targetStructure = target->structure();
            if (targetStructure->canPerformFastPropertyEnumerationCommon()) {
                canUseFastPath = true;
                targetStructure->forEachProperty(vm, [&](const PropertyTableEntry& entry) -> bool {
                    if (entry.attributes() & PropertyAttribute::DontEnum)
                        return true;

                    if (entry.key()->isSymbol())
                        return true;

                    properties.append(entry.key());
                    values.appendWithCrashOnOverflow(target->getDirect(entry.offset()));

                    return true;
                });
            }
        }

        if (canUseFastPath) {
            Structure* arrayStructure = globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithContiguous);
            JSArray* entries = JSArray::tryCreate(vm, arrayStructure, properties.size());
            if (UNLIKELY(!entries)) {
                throwOutOfMemoryError(globalObject, scope);
                return { };
            }

            Structure* targetStructure = target->structure();
            JSImmutableButterfly* cachedButterfly = nullptr;
            if (LIKELY(!globalObject->isHavingABadTime())) {
                auto* butterfly = targetStructure->cachedPropertyNames(CachedPropertyNamesKind::EnumerableStrings);
                if (butterfly) {
                    ASSERT(butterfly->length() == properties.size());
                    if (butterfly->length() == properties.size())
                        cachedButterfly = butterfly;
                }
            }

            if (!cachedButterfly && properties.size() < MIN_SPARSE_ARRAY_INDEX && !globalObject->isHavingABadTime() && targetStructure->canCacheOwnPropertyNames()) {
                auto* canSentinel = targetStructure->cachedPropertyNamesIgnoringSentinel(CachedPropertyNamesKind::EnumerableStrings);
                if (canSentinel == StructureRareData::cachedPropertyNamesSentinel()) {
                    size_t numProperties = properties.size();
                    auto* newButterfly = JSImmutableButterfly::create(vm, CopyOnWriteArrayWithContiguous, numProperties);
                    for (size_t i = 0; i < numProperties; i++) {
                        const auto& identifier = properties[i];
                        newButterfly->setIndex(vm, i, jsOwnedString(vm, identifier.get()));
                    }

                    targetStructure->setCachedPropertyNames(vm, CachedPropertyNamesKind::EnumerableStrings, newButterfly);
                    cachedButterfly = newButterfly;
                } else
                    targetStructure->setCachedPropertyNames(vm, CachedPropertyNamesKind::EnumerableStrings, StructureRareData::cachedPropertyNamesSentinel());
            }

            for (size_t i = 0; i < properties.size(); ++i) {
                JSString* key = nullptr;
                if (cachedButterfly) {
                    auto* cachedKey = asString(cachedButterfly->get(i));
                    if (cachedKey->tryGetValueImpl() == properties[i].get())
                        key = cachedKey;
                }

                if (!key)
                    key = jsOwnedString(vm, properties[i].get());

                JSArray* entry = nullptr;
                {
                    ObjectInitializationScope initializationScope(vm);
                    if (LIKELY(entry = JSArray::tryCreateUninitializedRestricted(initializationScope, nullptr, globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithContiguous), 2))) {
                        entry->initializeIndex(initializationScope, 0, key);
                        entry->initializeIndex(initializationScope, 1, values.at(i));
                    }
                }
                if (UNLIKELY(!entry)) {
                    throwOutOfMemoryError(globalObject, scope);
                    return { };
                }
                entries->putDirectIndex(globalObject, i, entry);
                RETURN_IF_EXCEPTION(scope, { });
            }

            return JSValue::encode(entries);
        }
    }

    JSArray* entries = constructEmptyArray(globalObject, nullptr);
    RETURN_IF_EXCEPTION(scope, { });

    PropertyNameArray properties(vm, PropertyNameMode::Strings, PrivateSymbolMode::Exclude);
    target->methodTable()->getOwnPropertyNames(target, globalObject, properties, DontEnumPropertiesMode::Include);
    RETURN_IF_EXCEPTION(scope, { });

    unsigned index = 0;
    auto append = [&] (JSGlobalObject* globalObject, PropertyName propertyName) {
        PropertySlot slot(target, PropertySlot::InternalMethodType::GetOwnProperty);
        bool hasProperty = target->methodTable()->getOwnPropertySlot(target, globalObject, propertyName, slot);
        RETURN_IF_EXCEPTION(scope, void());
        if (!hasProperty)
            return;
        if (slot.attributes() & PropertyAttribute::DontEnum)
            return;

        JSValue value;
        if (LIKELY(!slot.isTaintedByOpaqueObject()))
            value = slot.getValue(globalObject, propertyName);
        else
            value = target->get(globalObject, propertyName);
        RETURN_IF_EXCEPTION(scope, void());

        JSString* key = jsOwnedString(vm, propertyName.uid());
        JSArray* entry = nullptr;
        {
            ObjectInitializationScope initializationScope(vm);
            if (LIKELY(entry = JSArray::tryCreateUninitializedRestricted(initializationScope, nullptr, globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithContiguous), 2))) {
                entry->initializeIndex(initializationScope, 0, key);
                entry->initializeIndex(initializationScope, 1, value);
            }
        }
        if (UNLIKELY(!entry)) {
            throwOutOfMemoryError(globalObject, scope);
            return;
        }
        entries->putDirectIndex(globalObject, index++, entry);
    };

    for (const auto& propertyName : properties) {
        append(globalObject, propertyName);
        RETURN_IF_EXCEPTION(scope, { });
    }

    return JSValue::encode(entries);
}

JSC_DEFINE_HOST_FUNCTION(objectConstructorValues, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue targetValue = callFrame->argument(0);
    if (targetValue.isUndefinedOrNull())
        return throwVMTypeError(globalObject, scope, "Object.values requires that input parameter not be null or undefined"_s);
    JSObject* target = targetValue.toObject(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    if (!target->staticPropertiesReified()) {
        target->reifyAllStaticProperties(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
    }

    {
        MarkedArgumentBuffer namedPropertyValues;
        bool canUseFastPath = false;
        if (!target->canHaveExistingOwnIndexedGetterSetterProperties() && !target->hasNonReifiedStaticProperties()) {
            Structure* targetStructure = target->structure();
            if (targetStructure->canPerformFastPropertyEnumerationCommon()) {
                canUseFastPath = true;
                targetStructure->forEachProperty(vm, [&](const PropertyTableEntry& entry) -> bool {
                    if (entry.attributes() & PropertyAttribute::DontEnum)
                        return true;

                    if (entry.key()->isSymbol())
                        return true;

                    namedPropertyValues.appendWithCrashOnOverflow(target->getDirect(entry.offset()));
                    return true;
                });
            }
        }

        if (canUseFastPath) {
            Structure* arrayStructure = globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithContiguous);
            MarkedArgumentBuffer indexedPropertyValues;
            if (target->canHaveExistingOwnIndexedProperties()) {
                target->forEachOwnIndexedProperty<JSObject::SortMode::Ascending>(globalObject, [&](unsigned, JSValue value) {
                    indexedPropertyValues.appendWithCrashOnOverflow(value);
                    return IterationStatus::Continue;
                });
            }
            RETURN_IF_EXCEPTION(scope, { });

            {
                ObjectInitializationScope initializationScope(vm);
                JSArray* result = nullptr;
                if (LIKELY(result = JSArray::tryCreateUninitializedRestricted(initializationScope, nullptr, arrayStructure, indexedPropertyValues.size() + namedPropertyValues.size()))) {
                    for (unsigned i = 0; i < indexedPropertyValues.size(); ++i)
                        result->initializeIndex(initializationScope, i, indexedPropertyValues.at(i));
                    for (unsigned i = 0; i < namedPropertyValues.size(); ++i)
                        result->initializeIndex(initializationScope, indexedPropertyValues.size() + i, namedPropertyValues.at(i));
                    return JSValue::encode(result);
                }
            }
            throwOutOfMemoryError(globalObject, scope);
            return { };
        }
    }

    JSArray* values = constructEmptyArray(globalObject, nullptr);
    RETURN_IF_EXCEPTION(scope, { });

    PropertyNameArray properties(vm, PropertyNameMode::Strings, PrivateSymbolMode::Exclude);
    target->methodTable()->getOwnPropertyNames(target, globalObject, properties, DontEnumPropertiesMode::Include);
    RETURN_IF_EXCEPTION(scope, { });

    unsigned index = 0;
    auto append = [&] (JSGlobalObject* globalObject, PropertyName propertyName) {
        PropertySlot slot(target, PropertySlot::InternalMethodType::GetOwnProperty);
        bool hasProperty = target->methodTable()->getOwnPropertySlot(target, globalObject, propertyName, slot);
        RETURN_IF_EXCEPTION(scope, void());
        if (!hasProperty)
            return;
        if (slot.attributes() & PropertyAttribute::DontEnum)
            return;

        JSValue value;
        if (LIKELY(!slot.isTaintedByOpaqueObject()))
            value = slot.getValue(globalObject, propertyName);
        else
            value = target->get(globalObject, propertyName);
        RETURN_IF_EXCEPTION(scope, void());

        values->putDirectIndex(globalObject, index++, value);
    };

    for (const auto& propertyName : properties) {
        append(globalObject, propertyName);
        RETURN_IF_EXCEPTION(scope, { });
    }

    return JSValue::encode(values);
}

// https://tc39.github.io/ecma262/#sec-topropertydescriptor
inline bool toPropertyDescriptor(JSGlobalObject* globalObject, JSValue in, PropertyDescriptor& desc, bool& withoutSideEffect)
{
    ASSERT(desc.isEmpty());
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (UNLIKELY(!in.isObject())) {
        throwTypeError(globalObject, scope, "Property description must be an object."_s);
        return false;
    }
    JSObject* description = asObject(in);

    JSValue enumerable;
    JSValue configurable;
    JSValue value;
    JSValue writable;
    JSValue get;
    JSValue set;

    if (globalObject->propertyDescriptorFastPathWatchpointSet().state() == ClearWatchpoint)
        globalObject->tryInstallPropertyDescriptorFastPathWatchpoint();


    bool canUseFastPath = false;
    if (globalObject->propertyDescriptorFastPathWatchpointSet().isStillValid() && globalObject->objectPrototypeChainIsSane() && description->inherits<JSFinalObject>() && description->getPrototypeDirect() == globalObject->objectPrototype() && !description->hasNonReifiedStaticProperties()) {
        Structure* descriptionStructure = description->structure();
        if (descriptionStructure->canPerformFastPropertyEnumeration()) {
            canUseFastPath = true;
            descriptionStructure->forEachProperty(vm, [&](const PropertyTableEntry& entry) -> bool {
                PropertyName propertyName(entry.key());
                if (propertyName == vm.propertyNames->enumerable)
                    enumerable = description->getDirect(entry.offset());
                else if (propertyName == vm.propertyNames->configurable)
                    configurable = description->getDirect(entry.offset());
                else if (propertyName == vm.propertyNames->value)
                    value = description->getDirect(entry.offset());
                else if (propertyName == vm.propertyNames->writable)
                    writable = description->getDirect(entry.offset());
                else if (propertyName == vm.propertyNames->get)
                    get = description->getDirect(entry.offset());
                else if (propertyName == vm.propertyNames->set)
                    set = description->getDirect(entry.offset());
                return true;
            });

            if (enumerable)
                desc.setEnumerable(enumerable.toBoolean(globalObject));
            if (configurable)
                desc.setConfigurable(configurable.toBoolean(globalObject));
            if (value)
                desc.setValue(value);
            if (writable)
                desc.setWritable(writable.toBoolean(globalObject));
            if (get) {
                if (!get.isUndefined() && !get.isCallable()) {
                    throwTypeError(globalObject, scope, "Getter must be a function."_s);
                    return false;
                }
                desc.setGetter(get);
            }
            if (set) {
                if (!set.isUndefined() && !set.isCallable()) {
                    throwTypeError(globalObject, scope, "Setter must be a function."_s);
                    return false;
                }
                desc.setSetter(set);
            }
            withoutSideEffect = true;
        }
    }

    if (!canUseFastPath) {
        enumerable = description->getIfPropertyExists(globalObject, vm.propertyNames->enumerable);
        RETURN_IF_EXCEPTION(scope, false);
        if (enumerable)
            desc.setEnumerable(enumerable.toBoolean(globalObject));

        configurable = description->getIfPropertyExists(globalObject, vm.propertyNames->configurable);
        RETURN_IF_EXCEPTION(scope, false);
        if (configurable)
            desc.setConfigurable(configurable.toBoolean(globalObject));

        value = description->getIfPropertyExists(globalObject, vm.propertyNames->value);
        RETURN_IF_EXCEPTION(scope, false);
        if (value)
            desc.setValue(value);

        writable = description->getIfPropertyExists(globalObject, vm.propertyNames->writable);
        RETURN_IF_EXCEPTION(scope, false);
        if (writable)
            desc.setWritable(writable.toBoolean(globalObject));

        get = description->getIfPropertyExists(globalObject, vm.propertyNames->get);
        RETURN_IF_EXCEPTION(scope, false);
        if (get) {
            if (!get.isUndefined() && !get.isCallable()) {
                throwTypeError(globalObject, scope, "Getter must be a function."_s);
                return false;
            }
            desc.setGetter(get);
        }

        set = description->getIfPropertyExists(globalObject, vm.propertyNames->set);
        RETURN_IF_EXCEPTION(scope, false);
        if (set) {
            if (!set.isUndefined() && !set.isCallable()) {
                throwTypeError(globalObject, scope, "Setter must be a function."_s);
                return false;
            }
            desc.setSetter(set);
        }
    }

    if (!desc.isAccessorDescriptor())
        return true;

    if (desc.value()) {
        throwTypeError(globalObject, scope, "Invalid property.  'value' present on property with getter or setter."_s);
        return false;
    }

    if (desc.writablePresent()) {
        throwTypeError(globalObject, scope, "Invalid property.  'writable' present on property with getter or setter."_s);
        return false;
    }

    return true;
}

bool toPropertyDescriptor(JSGlobalObject* globalObject, JSValue value, PropertyDescriptor& desc)
{
    bool withoutSideEffect = false;
    return toPropertyDescriptor(globalObject, value, desc, withoutSideEffect);
}

JSC_DEFINE_HOST_FUNCTION(objectConstructorDefineProperty, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!callFrame->argument(0).isObject())
        return throwVMTypeError(globalObject, scope, "Properties can only be defined on Objects."_s);
    JSObject* obj = asObject(callFrame->argument(0));
    auto propertyName = callFrame->argument(1).toPropertyKey(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    PropertyDescriptor descriptor;
    auto success = toPropertyDescriptor(globalObject, callFrame->argument(2), descriptor);
    EXCEPTION_ASSERT(!scope.exception() == success);
    if (!success)
        return JSValue::encode(jsNull());
    ASSERT((descriptor.attributes() & PropertyAttribute::Accessor) || (!descriptor.isAccessorDescriptor()));
    scope.assertNoException();
    obj->methodTable()->defineOwnProperty(obj, globalObject, propertyName, descriptor, true);
    RELEASE_AND_RETURN(scope, JSValue::encode(obj));
}

static JSValue definePropertiesSlow(JSGlobalObject* globalObject, JSObject* object, JSObject* properties)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    PropertyNameArray propertyNames(vm, PropertyNameMode::StringsAndSymbols, PrivateSymbolMode::Exclude);
    asObject(properties)->methodTable()->getOwnPropertyNames(asObject(properties), globalObject, propertyNames, DontEnumPropertiesMode::Exclude);
    RETURN_IF_EXCEPTION(scope, { });
    size_t numProperties = propertyNames.size();
    Vector<PropertyDescriptor> descriptors;
    MarkedArgumentBuffer markBuffer;
#define RETURN_IF_EXCEPTION_CLEARING_OVERFLOW(value) do { \
    if (UNLIKELY(scope.exception())) { \
        markBuffer.overflowCheckNotNeeded(); \
        return value; \
    } \
} while (false)
    for (size_t i = 0; i < numProperties; i++) {
        JSValue prop = properties->get(globalObject, propertyNames[i]);
        RETURN_IF_EXCEPTION_CLEARING_OVERFLOW({ });
        PropertyDescriptor descriptor;
        toPropertyDescriptor(globalObject, prop, descriptor);
        RETURN_IF_EXCEPTION_CLEARING_OVERFLOW({ });
        descriptors.append(descriptor);
        // Ensure we mark all the values that we're accumulating
        if (descriptor.isDataDescriptor() && descriptor.value())
            markBuffer.append(descriptor.value());
        if (descriptor.isAccessorDescriptor()) {
            if (descriptor.getter())
                markBuffer.append(descriptor.getter());
            if (descriptor.setter())
                markBuffer.append(descriptor.setter());
        }
    }
    RELEASE_ASSERT(!markBuffer.hasOverflowed());
#undef RETURN_IF_EXCEPTION_CLEARING_OVERFLOW
    for (size_t i = 0; i < numProperties; i++) {
        auto& propertyName = propertyNames[i];
        ASSERT(!propertyName.isPrivateName());

        object->methodTable()->defineOwnProperty(object, globalObject, propertyName, descriptors[i], true);
        RETURN_IF_EXCEPTION(scope, { });
    }
    return object;
}

static JSValue defineProperties(JSGlobalObject* globalObject, JSObject* object, JSObject* properties)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    Vector<RefPtr<UniquedStringImpl>, 8> propertyNames;
    MarkedArgumentBuffer values;
    bool canUseFastPath = false;
    if (!hasIndexedProperties(properties->indexingType())) {
        Structure* propertiesStructure = properties->structure();
        if (!properties->hasNonReifiedStaticProperties() && propertiesStructure->canPerformFastPropertyEnumerationCommon()) {
            canUseFastPath = true;
            propertiesStructure->forEachProperty(vm, [&](const PropertyTableEntry& entry) -> bool {
                if (entry.attributes() & PropertyAttribute::DontEnum)
                    return true;

                PropertyName propertyName(entry.key());
                if (propertyName.isPrivateName())
                    return true;

                propertyNames.append(entry.key());
                values.appendWithCrashOnOverflow(properties->getDirect(entry.offset()));

                return true;
            });
        }
    }
    if (UNLIKELY(!canUseFastPath))
        RELEASE_AND_RETURN(scope, definePropertiesSlow(globalObject, object, properties));

    unsigned index = 0;
    unsigned numProperties = propertyNames.size();
    Vector<PropertyDescriptor, 16> descriptors;
    MarkedArgumentBuffer markBuffer;

    descriptors.reserveInitialCapacity(numProperties);

#define RETURN_IF_EXCEPTION_CLEARING_OVERFLOW(value) do { \
    if (UNLIKELY(scope.exception())) { \
        markBuffer.overflowCheckNotNeeded(); \
        return value; \
    } \
} while (false)

    for (; index < numProperties; ++index) {
        JSValue prop = values.at(index);
        bool withoutSideEffect = false;
        PropertyDescriptor descriptor;
        toPropertyDescriptor(globalObject, prop, descriptor, withoutSideEffect);
        RETURN_IF_EXCEPTION_CLEARING_OVERFLOW({ });
        descriptors.append(descriptor);
        // Ensure we mark all the values that we're accumulating
        if (descriptor.isDataDescriptor() && descriptor.value())
            markBuffer.append(descriptor.value());
        if (descriptor.isAccessorDescriptor()) {
            if (descriptor.getter())
                markBuffer.append(descriptor.getter());
            if (descriptor.setter())
                markBuffer.append(descriptor.setter());
        }
        if (UNLIKELY(!withoutSideEffect)) {
            // Bail out to the slow code.
            ++index;
            break;
        }
    }

    if (UNLIKELY(index < numProperties)) {
        for (; index < numProperties; ++index) {
            JSValue prop = properties->get(globalObject, propertyNames[index].get());
            RETURN_IF_EXCEPTION_CLEARING_OVERFLOW({ });
            PropertyDescriptor descriptor;
            toPropertyDescriptor(globalObject, prop, descriptor);
            RETURN_IF_EXCEPTION_CLEARING_OVERFLOW({ });
            descriptors.append(descriptor);
            // Ensure we mark all the values that we're accumulating
            if (descriptor.isDataDescriptor() && descriptor.value())
                markBuffer.append(descriptor.value());
            if (descriptor.isAccessorDescriptor()) {
                if (descriptor.getter())
                    markBuffer.append(descriptor.getter());
                if (descriptor.setter())
                    markBuffer.append(descriptor.setter());
            }
        }
    }

    RELEASE_ASSERT(!markBuffer.hasOverflowed());
#undef RETURN_IF_EXCEPTION_CLEARING_OVERFLOW

    for (unsigned index = 0; index < numProperties; index++) {
        PropertyName propertyName(propertyNames[index].get());
        ASSERT(!propertyName.isPrivateName());
        object->methodTable()->defineOwnProperty(object, globalObject, propertyName, descriptors[index], true);
        RETURN_IF_EXCEPTION(scope, { });
    }
    return object;
}

JSC_DEFINE_HOST_FUNCTION(objectConstructorDefineProperties, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!callFrame->argument(0).isObject())
        return throwVMTypeError(globalObject, scope, "Properties can only be defined on Objects."_s);
    JSObject* targetObj = asObject(callFrame->argument(0));
    JSObject* props = callFrame->argument(1).toObject(globalObject);
    EXCEPTION_ASSERT(!!scope.exception() == !props);
    if (UNLIKELY(!props))
        return encodedJSValue();
    RELEASE_AND_RETURN(scope, JSValue::encode(defineProperties(globalObject, targetObj, props)));
}

JSC_DEFINE_HOST_FUNCTION(objectConstructorCreate, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue proto = callFrame->argument(0);
    if (!proto.isObject() && !proto.isNull())
        return throwVMTypeError(globalObject, scope, "Object prototype may only be an Object or null."_s);
    JSObject* newObject = proto.isObject()
        ? constructEmptyObject(globalObject, asObject(proto))
        : constructEmptyObject(vm, globalObject->nullPrototypeObjectStructure());
    if (callFrame->argument(1).isUndefined())
        return JSValue::encode(newObject);
    JSObject* properties = callFrame->uncheckedArgument(1).toObject(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope, JSValue::encode(defineProperties(globalObject, newObject, properties)));
}

enum class IntegrityLevel {
    Sealed,
    Frozen
};

template<IntegrityLevel level>
bool setIntegrityLevel(JSGlobalObject* globalObject, VM& vm, JSObject* object)
{
    // See https://tc39.github.io/ecma262/#sec-setintegritylevel.
    auto scope = DECLARE_THROW_SCOPE(vm);

    bool success = object->methodTable()->preventExtensions(object, globalObject);
    RETURN_IF_EXCEPTION(scope, false);
    if (UNLIKELY(!success))
        return false;

    PropertyNameArray properties(vm, PropertyNameMode::StringsAndSymbols, PrivateSymbolMode::Exclude);
    object->methodTable()->getOwnPropertyNames(object, globalObject, properties, DontEnumPropertiesMode::Include);
    RETURN_IF_EXCEPTION(scope, false);

    PropertyNameArray::const_iterator end = properties.end();
    for (PropertyNameArray::const_iterator iter = properties.begin(); iter != end; ++iter) {
        auto& propertyName = *iter;
        ASSERT(!propertyName.isPrivateName());

        PropertyDescriptor desc;
        if (level == IntegrityLevel::Sealed)
            desc.setConfigurable(false);
        else {
            PropertyDescriptor currentDesc;
            bool hasPropertyDescriptor = object->getOwnPropertyDescriptor(globalObject, propertyName, currentDesc);
            RETURN_IF_EXCEPTION(scope, false);
            if (!hasPropertyDescriptor)
                continue;

            if (!currentDesc.isAccessorDescriptor())
                desc.setWritable(false);

            desc.setConfigurable(false);
        }

        object->methodTable()->defineOwnProperty(object, globalObject, propertyName, desc, true);
        RETURN_IF_EXCEPTION(scope, false);
    }
    return true;
}

template<IntegrityLevel level>
bool testIntegrityLevel(JSGlobalObject* globalObject, VM& vm, JSObject* object)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 1. Assert: Type(O) is Object.
    // 2. Assert: level is either "sealed" or "frozen".

    // 3. Let status be ?IsExtensible(O).
    bool status = object->isExtensible(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    // 4. If status is true, return false.
    if (status)
        return false;

    // 6. Let keys be ? O.[[OwnPropertyKeys]]().
    PropertyNameArray keys(vm, PropertyNameMode::StringsAndSymbols, PrivateSymbolMode::Exclude);
    object->methodTable()->getOwnPropertyNames(object, globalObject, keys, DontEnumPropertiesMode::Include);
    RETURN_IF_EXCEPTION(scope, { });

    // 7. For each element k of keys, do
    PropertyNameArray::const_iterator end = keys.end();
    for (PropertyNameArray::const_iterator iter = keys.begin(); iter != end; ++iter) {
        auto& propertyName = *iter;
        ASSERT(!propertyName.isPrivateName());

        // a. Let currentDesc be ? O.[[GetOwnProperty]](k)
        PropertyDescriptor desc;
        bool didGetDescriptor = object->getOwnPropertyDescriptor(globalObject, propertyName, desc);
        RETURN_IF_EXCEPTION(scope, { });

        // b. If currentDesc is not undefined, then
        if (!didGetDescriptor)
            continue;

        // i. If currentDesc.[[Configurable]] is true, return false.
        if (desc.configurable())
            return false;

        // ii. If level is "frozen" and IsDataDescriptor(currentDesc) is true, then
        // 1. If currentDesc.[[Writable]] is true, return false.
        if (level == IntegrityLevel::Frozen && desc.isDataDescriptor() && desc.writable())
            return false;
    }

    return true;
}

JSObject* objectConstructorSeal(JSGlobalObject* globalObject, JSObject* object)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (jsDynamicCast<JSFinalObject*>(object) && !hasIndexedProperties(object->indexingType())) {
        object->seal(vm);
        return object;
    }

    bool success = setIntegrityLevel<IntegrityLevel::Sealed>(globalObject, vm, object);
    RETURN_IF_EXCEPTION(scope, nullptr);
    if (UNLIKELY(!success)) {
        throwTypeError(globalObject, scope, "Unable to prevent extension in Object.seal"_s);
        return nullptr;
    }

    return object;
}

JSC_DEFINE_HOST_FUNCTION(objectConstructorSeal, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 1. If Type(O) is not Object, return O.
    JSValue obj = callFrame->argument(0);
    if (!obj.isObject())
        return JSValue::encode(obj);

    RELEASE_AND_RETURN(scope, JSValue::encode(objectConstructorSeal(globalObject, asObject(obj))));
}

JSObject* objectConstructorFreeze(JSGlobalObject* globalObject, JSObject* object)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (jsDynamicCast<JSFinalObject*>(object) && !hasIndexedProperties(object->indexingType())) {
        object->freeze(vm);
        return object;
    }

    bool success = setIntegrityLevel<IntegrityLevel::Frozen>(globalObject, vm, object);
    RETURN_IF_EXCEPTION(scope, nullptr);
    if (UNLIKELY(!success)) {
        throwTypeError(globalObject, scope, "Unable to prevent extension in Object.freeze"_s);
        return nullptr;
    }
    return object;
}

JSC_DEFINE_HOST_FUNCTION(objectConstructorFreeze, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    // 1. If Type(O) is not Object, return O.
    JSValue obj = callFrame->argument(0);
    if (!obj.isObject())
        return JSValue::encode(obj);
    JSObject* result = objectConstructorFreeze(globalObject, asObject(obj));
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    return JSValue::encode(result);
}

JSC_DEFINE_HOST_FUNCTION(objectConstructorPreventExtensions, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue argument = callFrame->argument(0);
    if (!argument.isObject())
        return JSValue::encode(argument);
    JSObject* object = asObject(argument);
    bool status = object->methodTable()->preventExtensions(object, globalObject);
    RETURN_IF_EXCEPTION(scope, { });
    if (UNLIKELY(!status))
        return throwVMTypeError(globalObject, scope, "Unable to prevent extension in Object.preventExtensions"_s);
    return JSValue::encode(object);
}

JSC_DEFINE_HOST_FUNCTION(objectConstructorIsSealed, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();

    // 1. If Type(O) is not Object, return true.
    JSValue obj = callFrame->argument(0);
    if (!obj.isObject())
        return JSValue::encode(jsBoolean(true));
    JSObject* object = asObject(obj);

    // Quick check for final objects.
    if (jsDynamicCast<JSFinalObject*>(object) && !hasIndexedProperties(object->indexingType()))
        return JSValue::encode(jsBoolean(object->isSealed(vm)));

    // 2. Return ? TestIntegrityLevel(O, "sealed").
    return JSValue::encode(jsBoolean(testIntegrityLevel<IntegrityLevel::Sealed>(globalObject, vm, object)));
}

JSC_DEFINE_HOST_FUNCTION(objectConstructorIsFrozen, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();

    // 1. If Type(O) is not Object, return true.
    JSValue obj = callFrame->argument(0);
    if (!obj.isObject())
        return JSValue::encode(jsBoolean(true));
    JSObject* object = asObject(obj);

    // Quick check for final objects.
    if (jsDynamicCast<JSFinalObject*>(object) && !hasIndexedProperties(object->indexingType()))
        return JSValue::encode(jsBoolean(object->isFrozen(vm)));

    // 2. Return ? TestIntegrityLevel(O, "frozen").
    return JSValue::encode(jsBoolean(testIntegrityLevel<IntegrityLevel::Frozen>(globalObject, vm, object)));
}

JSC_DEFINE_HOST_FUNCTION(objectConstructorIsExtensible, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue obj = callFrame->argument(0);
    if (!obj.isObject())
        return JSValue::encode(jsBoolean(false));
    JSObject* object = asObject(obj);
    bool isExtensible = object->isExtensible(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    return JSValue::encode(jsBoolean(isExtensible));
}

JSC_DEFINE_HOST_FUNCTION(objectConstructorIs, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return JSValue::encode(jsBoolean(sameValue(globalObject, callFrame->argument(0), callFrame->argument(1))));
}

static CachedPropertyNamesKind inferCachedPropertyNamesKind(PropertyNameMode propertyNameMode, DontEnumPropertiesMode dontEnumPropertiesMode)
{
    switch (propertyNameMode) {
    case PropertyNameMode::Strings:
        return dontEnumPropertiesMode == DontEnumPropertiesMode::Include ? CachedPropertyNamesKind::Strings : CachedPropertyNamesKind::EnumerableStrings;
    case PropertyNameMode::Symbols:
        ASSERT(dontEnumPropertiesMode == DontEnumPropertiesMode::Include);
        return CachedPropertyNamesKind::Symbols;
    case PropertyNameMode::StringsAndSymbols:
        ASSERT(dontEnumPropertiesMode == DontEnumPropertiesMode::Include);
        return CachedPropertyNamesKind::StringsAndSymbols;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

JSArray* ownPropertyKeys(JSGlobalObject* globalObject, JSObject* object, PropertyNameMode propertyNameMode, DontEnumPropertiesMode dontEnumPropertiesMode)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto kind = inferCachedPropertyNamesKind(propertyNameMode, dontEnumPropertiesMode);

    if (object->inherits<ProxyObject>()) {
        ProxyObject* proxy = jsCast<ProxyObject*>(object);
        if (proxy->forwardsGetOwnPropertyNamesToTarget(dontEnumPropertiesMode))
            object = proxy->target();
    }

    // We attempt to look up own property keys cache in Object.keys / Object.getOwnPropertyNames cases.
    if (LIKELY(!globalObject->isHavingABadTime())) {
        if (auto* immutableButterfly = object->structure()->cachedPropertyNames(kind)) {
            Structure* arrayStructure = globalObject->originalArrayStructureForIndexingType(immutableButterfly->indexingMode());
            return JSArray::createWithButterfly(vm, nullptr, arrayStructure, immutableButterfly->toButterfly());
        }
    }

    PropertyNameArray properties(vm, propertyNameMode, PrivateSymbolMode::Exclude);
    object->methodTable()->getOwnPropertyNames(object, globalObject, properties, dontEnumPropertiesMode);
    RETURN_IF_EXCEPTION(scope, nullptr);

    size_t numProperties = properties.size();
    if (LIKELY(numProperties < MIN_SPARSE_ARRAY_INDEX) && !globalObject->isHavingABadTime()) {
        auto copyPropertiesToBuffer = [&](WriteBarrier<Unknown>* buffer, JSCell* owner) {
            for (size_t i = 0; i < numProperties; i++) {
                const auto& identifier = properties[i];
                if (propertyNameMode != PropertyNameMode::Strings && identifier.isSymbol()) {
                    ASSERT(!identifier.isPrivateName());
                    buffer[i].set(vm, owner, Symbol::create(vm, static_cast<SymbolImpl&>(*identifier.impl())));
                } else
                    buffer[i].set(vm, owner, jsOwnedString(vm, identifier.string()));
            }
        };

        Structure* structure = object->structure();
        if (structure->canCacheOwnPropertyNames()) {
            auto* cachedButterfly = structure->cachedPropertyNamesIgnoringSentinel(kind);
            if (cachedButterfly == StructureRareData::cachedPropertyNamesSentinel()) {
                auto* newButterfly = JSImmutableButterfly::create(vm, CopyOnWriteArrayWithContiguous, numProperties);
                copyPropertiesToBuffer(newButterfly->toButterfly()->contiguous().data(), newButterfly);

                structure->setCachedPropertyNames(vm, kind, newButterfly);
                Structure* arrayStructure = globalObject->originalArrayStructureForIndexingType(newButterfly->indexingMode());
                return JSArray::createWithButterfly(vm, nullptr, arrayStructure, newButterfly->toButterfly());
            }

            if (cachedButterfly == nullptr)
                structure->setCachedPropertyNames(vm, kind, StructureRareData::cachedPropertyNamesSentinel());
        }

        // FIXME: We should probably be calling tryCreate here:
        // https://bugs.webkit.org/show_bug.cgi?id=221984
        JSArray* keys = JSArray::create(vm, globalObject->originalArrayStructureForIndexingType(ArrayWithContiguous), numProperties);
        copyPropertiesToBuffer(keys->butterfly()->contiguous().data(), keys);

        return keys;
    }

    JSArray* keys = constructEmptyArray(globalObject, nullptr);
    RETURN_IF_EXCEPTION(scope, nullptr);

    unsigned index = 0;
    auto pushDirect = [&] (JSGlobalObject* globalObject, JSArray* array, JSValue value) {
        array->putDirectIndex(globalObject, index++, value);
    };

    switch (propertyNameMode) {
    case PropertyNameMode::Strings: {
        for (size_t i = 0; i < numProperties; i++) {
            const auto& identifier = properties[i];
            ASSERT(!identifier.isSymbol());
            pushDirect(globalObject, keys, jsOwnedString(vm, identifier.string()));
            RETURN_IF_EXCEPTION(scope, nullptr);
        }
        break;
    }

    case PropertyNameMode::Symbols: {
        for (size_t i = 0; i < numProperties; i++) {
            const auto& identifier = properties[i];
            ASSERT(identifier.isSymbol());
            ASSERT(!identifier.isPrivateName());
            pushDirect(globalObject, keys, Symbol::create(vm, static_cast<SymbolImpl&>(*identifier.impl())));
            RETURN_IF_EXCEPTION(scope, nullptr);
        }
        break;
    }

    case PropertyNameMode::StringsAndSymbols: {
        for (size_t i = 0; i < numProperties; i++) {
            const auto& identifier = properties[i];
            if (identifier.isSymbol()) {
                ASSERT(!identifier.isPrivateName());
                pushDirect(globalObject, keys, Symbol::create(vm, static_cast<SymbolImpl&>(*identifier.impl())));
            } else
                pushDirect(globalObject, keys, jsOwnedString(vm, identifier.string()));
            RETURN_IF_EXCEPTION(scope, nullptr);
        }
        break;
    }
    }

    return keys;
}

JSObject* constructObjectFromPropertyDescriptorSlow(JSGlobalObject* globalObject, const PropertyDescriptor& descriptor)
{
    VM& vm = getVM(globalObject);

    JSObject* result = constructEmptyObject(globalObject);

    if (descriptor.value())
        result->putDirect(vm, vm.propertyNames->value, descriptor.value());
    if (descriptor.writablePresent())
        result->putDirect(vm, vm.propertyNames->writable, jsBoolean(descriptor.writable()));
    if (descriptor.getterPresent())
        result->putDirect(vm, vm.propertyNames->get, descriptor.getter());
    if (descriptor.setterPresent())
        result->putDirect(vm, vm.propertyNames->set, descriptor.setter());
    if (descriptor.enumerablePresent())
        result->putDirect(vm, vm.propertyNames->enumerable, jsBoolean(descriptor.enumerable()));
    if (descriptor.configurablePresent())
        result->putDirect(vm, vm.propertyNames->configurable, jsBoolean(descriptor.configurable()));

    return result;
}

JSC_DEFINE_HOST_FUNCTION(objectConstructorHasOwn, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSObject* base = callFrame->argument(0).toObject(globalObject);
    RETURN_IF_EXCEPTION(scope, { });
    auto propertyName = callFrame->argument(1).toPropertyKey(globalObject);
    RETURN_IF_EXCEPTION(scope, { });
    RELEASE_AND_RETURN(scope, JSValue::encode(jsBoolean(objectPrototypeHasOwnProperty(globalObject, base, propertyName))));
}

} // namespace JSC
