/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2008-2019 Apple Inc. All rights reserved.
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
#include "PropertyDescriptor.h"
#include "PropertyNameArray.h"
#include "Symbol.h"

namespace JSC {

JSC_DECLARE_HOST_FUNCTION(objectConstructorAssign);
JSC_DECLARE_HOST_FUNCTION(objectConstructorValues);
JSC_DECLARE_HOST_FUNCTION(objectConstructorGetPrototypeOf);
JSC_DECLARE_HOST_FUNCTION(objectConstructorSetPrototypeOf);
JSC_DECLARE_HOST_FUNCTION(objectConstructorGetOwnPropertyNames);
JSC_DECLARE_HOST_FUNCTION(objectConstructorDefineProperty);
JSC_DECLARE_HOST_FUNCTION(objectConstructorDefineProperties);
JSC_DECLARE_HOST_FUNCTION(objectConstructorCreate);
JSC_DECLARE_HOST_FUNCTION(objectConstructorSeal);
JSC_DECLARE_HOST_FUNCTION(objectConstructorFreeze);
JSC_DECLARE_HOST_FUNCTION(objectConstructorPreventExtensions);
JSC_DECLARE_HOST_FUNCTION(objectConstructorIsSealed);
JSC_DECLARE_HOST_FUNCTION(objectConstructorIsFrozen);
JSC_DECLARE_HOST_FUNCTION(objectConstructorIsExtensible);

}

#include "ObjectConstructor.lut.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(ObjectConstructor);

const ClassInfo ObjectConstructor::s_info = { "Function", &InternalFunction::s_info, &objectConstructorTable, nullptr, CREATE_METHOD_TABLE(ObjectConstructor) };

/* Source for ObjectConstructor.lut.h
@begin objectConstructorTable
  getPrototypeOf            objectConstructorGetPrototypeOf             DontEnum|Function 1 ObjectGetPrototypeOfIntrinsic
  setPrototypeOf            objectConstructorSetPrototypeOf             DontEnum|Function 2
  getOwnPropertyDescriptor  objectConstructorGetOwnPropertyDescriptor   DontEnum|Function 2
  getOwnPropertyDescriptors objectConstructorGetOwnPropertyDescriptors  DontEnum|Function 1
  getOwnPropertyNames       objectConstructorGetOwnPropertyNames        DontEnum|Function 1 ObjectGetOwnPropertyNamesIntrinsic
  getOwnPropertySymbols     objectConstructorGetOwnPropertySymbols      DontEnum|Function 1
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
  assign                    objectConstructorAssign                     DontEnum|Function 2
  values                    objectConstructorValues                     DontEnum|Function 1
  entries                   JSBuiltin                                   DontEnum|Function 1
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

    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().createPrivateName(), objectConstructorCreate, static_cast<unsigned>(PropertyAttribute::DontEnum), 2);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().definePropertyPrivateName(), objectConstructorDefineProperty, static_cast<unsigned>(PropertyAttribute::DontEnum), 3);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().getOwnPropertyNamesPrivateName(), objectConstructorGetOwnPropertyNames, static_cast<unsigned>(PropertyAttribute::DontEnum), 1);
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
        Structure* baseStructure = getFunctionRealm(vm, asObject(newTarget))->objectStructureForObjectConstructor();
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
        return throwVMTypeError(globalObject, scope, "Prototype value can only be an object or null"_s);

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
    object->methodTable(vm)->getOwnPropertyNames(object, globalObject, properties, EnumerationMode(DontEnumPropertiesMode::Include));
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
        scope.assertNoException();
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
    RELEASE_AND_RETURN(scope, JSValue::encode(ownPropertyKeys(globalObject, object, PropertyNameMode::Strings, DontEnumPropertiesMode::Include, CachedPropertyNamesKind::GetOwnPropertyNames)));
}

// FIXME: Use the enumeration cache.
JSC_DEFINE_HOST_FUNCTION(objectConstructorGetOwnPropertySymbols, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSObject* object = callFrame->argument(0).toObject(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    RELEASE_AND_RETURN(scope, JSValue::encode(ownPropertyKeys(globalObject, object, PropertyNameMode::Symbols, DontEnumPropertiesMode::Include, WTF::nullopt)));
}

JSC_DEFINE_HOST_FUNCTION(objectConstructorKeys, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSObject* object = callFrame->argument(0).toObject(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    RELEASE_AND_RETURN(scope, JSValue::encode(ownPropertyKeys(globalObject, object, PropertyNameMode::Strings, DontEnumPropertiesMode::Exclude, CachedPropertyNamesKind::Keys)));
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
    bool targetCanPerformFastPut = jsDynamicCast<JSFinalObject*>(vm, target) && target->canPerformFastPutInlineExcludingProto(vm);

    Vector<RefPtr<UniquedStringImpl>, 8> properties;
    MarkedArgumentBuffer values;
    unsigned argsCount = callFrame->argumentCount();
    for (unsigned i = 1; i < argsCount; ++i) {
        JSValue sourceValue = callFrame->uncheckedArgument(i);
        if (sourceValue.isUndefinedOrNull())
            continue;
        JSObject* source = sourceValue.toObject(globalObject);
        RETURN_IF_EXCEPTION(scope, { });

        if (targetCanPerformFastPut) {
            if (!source->staticPropertiesReified(vm)) {
                source->reifyAllStaticProperties(globalObject);
                RETURN_IF_EXCEPTION(scope, { });
            }

            auto canPerformFastPropertyEnumerationForObjectAssign = [] (Structure* structure) {
                if (structure->typeInfo().overridesGetOwnPropertySlot())
                    return false;
                if (structure->typeInfo().overridesAnyFormOfGetPropertyNames())
                    return false;
                // FIXME: Indexed properties can be handled.
                // https://bugs.webkit.org/show_bug.cgi?id=185358
                if (hasIndexedProperties(structure->indexingType()))
                    return false;
                if (structure->hasGetterSetterProperties())
                    return false;
                if (structure->hasReadOnlyOrGetterSetterPropertiesExcludingProto())
                    return false;
                if (structure->hasCustomGetterSetterProperties())
                    return false;
                if (structure->isUncacheableDictionary())
                    return false;
                // Cannot perform fast [[Put]] to |target| if the property names of the |source| contain "__proto__".
                if (structure->hasUnderscoreProtoPropertyExcludingOriginalProto())
                    return false;
                return true;
            };

            if (canPerformFastPropertyEnumerationForObjectAssign(source->structure(vm))) {
                // |source| Structure does not have any getters. And target can perform fast put.
                // So enumerating properties and putting properties are non observable.

                // FIXME: It doesn't seem like we should have to do this in two phases, but
                // we're running into crashes where it appears that source is transitioning
                // under us, and even ends up in a state where it has a null butterfly. My
                // leading hypothesis here is that we fire some value replacement watchpoint
                // that ends up transitioning the structure underneath us.
                // https://bugs.webkit.org/show_bug.cgi?id=187837

                // Do not clear since Vector::clear shrinks the backing store.
                properties.resize(0);
                values.clear();
                source->structure(vm)->forEachProperty(vm, [&] (const PropertyMapEntry& entry) -> bool {
                    if (entry.attributes & PropertyAttribute::DontEnum)
                        return true;

                    PropertyName propertyName(entry.key);
                    if (propertyName.isPrivateName())
                        return true;

                    properties.append(entry.key);
                    values.appendWithCrashOnOverflow(source->getDirect(entry.offset));

                    return true;
                });

                for (size_t i = 0; i < properties.size(); ++i) {
                    // FIXME: We could put properties in a batching manner to accelerate Object.assign more.
                    // https://bugs.webkit.org/show_bug.cgi?id=185358
                    PutPropertySlot putPropertySlot(target, true);
                    target->putOwnDataProperty(vm, properties[i].get(), values.at(i), putPropertySlot);
                }
                continue;
            }
        }

        // [[GetOwnPropertyNames]], [[Get]] etc. could modify target object and invalidate this assumption.
        // For example, [[Get]] of source object could configure setter to target object. So disable the fast path.
        targetCanPerformFastPut = false;

        PropertyNameArray properties(vm, PropertyNameMode::StringsAndSymbols, PrivateSymbolMode::Exclude);
        source->methodTable(vm)->getOwnPropertyNames(source, globalObject, properties, EnumerationMode(DontEnumPropertiesMode::Include));
        RETURN_IF_EXCEPTION(scope, { });

        unsigned numProperties = properties.size();
        for (unsigned j = 0; j < numProperties; j++) {
            const auto& propertyName = properties[j];
            ASSERT(!propertyName.isPrivateName());

            PropertySlot slot(source, PropertySlot::InternalMethodType::GetOwnProperty);
            bool hasProperty = source->methodTable(vm)->getOwnPropertySlot(source, globalObject, propertyName, slot);
            RETURN_IF_EXCEPTION(scope, { });
            if (!hasProperty)
                continue;
            if (slot.attributes() & PropertyAttribute::DontEnum)
                continue;

            JSValue value;
            if (LIKELY(!slot.isTaintedByOpaqueObject()))
                value = slot.getValue(globalObject, propertyName);
            else
                value = source->get(globalObject, propertyName);
            RETURN_IF_EXCEPTION(scope, { });

            PutPropertySlot putPropertySlot(target, true);
            target->putInline(globalObject, propertyName, value, putPropertySlot);
            RETURN_IF_EXCEPTION(scope, { });
        }
    }
    return JSValue::encode(target);
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

    JSArray* values = constructEmptyArray(globalObject, nullptr);
    RETURN_IF_EXCEPTION(scope, { });

    PropertyNameArray properties(vm, PropertyNameMode::Strings, PrivateSymbolMode::Exclude);
    target->methodTable(vm)->getOwnPropertyNames(target, globalObject, properties, EnumerationMode(DontEnumPropertiesMode::Include));
    RETURN_IF_EXCEPTION(scope, { });

    unsigned index = 0;
    auto addValue = [&] (PropertyName propertyName) {
        PropertySlot slot(target, PropertySlot::InternalMethodType::GetOwnProperty);
        bool hasProperty = target->methodTable(vm)->getOwnPropertySlot(target, globalObject, propertyName, slot);
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

    for (unsigned i = 0, numProperties = properties.size(); i < numProperties; i++) {
        const auto& propertyName = properties[i];
        if (propertyName.isSymbol())
            continue;

        addValue(propertyName);
        RETURN_IF_EXCEPTION(scope, { });
    }

    return JSValue::encode(values);
}


// ES6 6.2.4.5 ToPropertyDescriptor
// https://tc39.github.io/ecma262/#sec-topropertydescriptor
bool toPropertyDescriptor(JSGlobalObject* globalObject, JSValue in, PropertyDescriptor& desc)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!in.isObject()) {
        throwTypeError(globalObject, scope, "Property description must be an object."_s);
        return false;
    }
    JSObject* description = asObject(in);

    bool hasProperty = description->hasProperty(globalObject, vm.propertyNames->enumerable);
    EXCEPTION_ASSERT(!scope.exception() || !hasProperty);
    if (hasProperty) {
        JSValue value = description->get(globalObject, vm.propertyNames->enumerable);
        RETURN_IF_EXCEPTION(scope, false);
        desc.setEnumerable(value.toBoolean(globalObject));
    } else
        RETURN_IF_EXCEPTION(scope, false);

    hasProperty = description->hasProperty(globalObject, vm.propertyNames->configurable);
    EXCEPTION_ASSERT(!scope.exception() || !hasProperty);
    if (hasProperty) {
        JSValue value = description->get(globalObject, vm.propertyNames->configurable);
        RETURN_IF_EXCEPTION(scope, false);
        desc.setConfigurable(value.toBoolean(globalObject));
    } else
        RETURN_IF_EXCEPTION(scope, false);

    JSValue value;
    hasProperty = description->hasProperty(globalObject, vm.propertyNames->value);
    EXCEPTION_ASSERT(!scope.exception() || !hasProperty);
    if (hasProperty) {
        JSValue value = description->get(globalObject, vm.propertyNames->value);
        RETURN_IF_EXCEPTION(scope, false);
        desc.setValue(value);
    } else
        RETURN_IF_EXCEPTION(scope, false);

    hasProperty = description->hasProperty(globalObject, vm.propertyNames->writable);
    EXCEPTION_ASSERT(!scope.exception() || !hasProperty);
    if (hasProperty) {
        JSValue value = description->get(globalObject, vm.propertyNames->writable);
        RETURN_IF_EXCEPTION(scope, false);
        desc.setWritable(value.toBoolean(globalObject));
    } else
        RETURN_IF_EXCEPTION(scope, false);

    hasProperty = description->hasProperty(globalObject, vm.propertyNames->get);
    EXCEPTION_ASSERT(!scope.exception() || !hasProperty);
    if (hasProperty) {
        JSValue get = description->get(globalObject, vm.propertyNames->get);
        RETURN_IF_EXCEPTION(scope, false);
        if (!get.isUndefined() && !get.isCallable(vm)) {
            throwTypeError(globalObject, scope, "Getter must be a function."_s);
            return false;
        }
        desc.setGetter(get);
    } else
        RETURN_IF_EXCEPTION(scope, false);

    hasProperty = description->hasProperty(globalObject, vm.propertyNames->set);
    EXCEPTION_ASSERT(!scope.exception() || !hasProperty);
    if (hasProperty) {
        JSValue set = description->get(globalObject, vm.propertyNames->set);
        RETURN_IF_EXCEPTION(scope, false);
        if (!set.isUndefined() && !set.isCallable(vm)) {
            throwTypeError(globalObject, scope, "Setter must be a function."_s);
            return false;
        }
        desc.setSetter(set);
    } else
        RETURN_IF_EXCEPTION(scope, false);

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
    obj->methodTable(vm)->defineOwnProperty(obj, globalObject, propertyName, descriptor, true);
    RELEASE_AND_RETURN(scope, JSValue::encode(obj));
}

static JSValue defineProperties(JSGlobalObject* globalObject, JSObject* object, JSObject* properties)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    PropertyNameArray propertyNames(vm, PropertyNameMode::StringsAndSymbols, PrivateSymbolMode::Exclude);
    asObject(properties)->methodTable(vm)->getOwnPropertyNames(asObject(properties), globalObject, propertyNames, EnumerationMode(DontEnumPropertiesMode::Exclude));
    RETURN_IF_EXCEPTION(scope, { });
    size_t numProperties = propertyNames.size();
    Vector<PropertyDescriptor> descriptors;
    MarkedArgumentBuffer markBuffer;
#define RETURN_IF_EXCEPTION_CLEARING_OVERFLOW(value) do { \
    if (scope.exception()) { \
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

        object->methodTable(vm)->defineOwnProperty(object, globalObject, propertyName, descriptors[i], true);
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

    bool success = object->methodTable(vm)->preventExtensions(object, globalObject);
    RETURN_IF_EXCEPTION(scope, false);
    if (UNLIKELY(!success))
        return false;

    PropertyNameArray properties(vm, PropertyNameMode::StringsAndSymbols, PrivateSymbolMode::Exclude);
    object->methodTable(vm)->getOwnPropertyNames(object, globalObject, properties, EnumerationMode(DontEnumPropertiesMode::Include));
    RETURN_IF_EXCEPTION(scope, false);

    PropertyNameArray::const_iterator end = properties.end();
    for (PropertyNameArray::const_iterator iter = properties.begin(); iter != end; ++iter) {
        auto& propertyName = *iter;
        ASSERT(!propertyName.isPrivateName());

        PropertyDescriptor desc;
        if (level == IntegrityLevel::Sealed)
            desc.setConfigurable(false);
        else {
            bool hasPropertyDescriptor = object->getOwnPropertyDescriptor(globalObject, propertyName, desc);
            RETURN_IF_EXCEPTION(scope, false);
            if (!hasPropertyDescriptor)
                continue;

            if (desc.isDataDescriptor())
                desc.setWritable(false);

            desc.setConfigurable(false);
        }

        object->methodTable(vm)->defineOwnProperty(object, globalObject, propertyName, desc, true);
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
    object->methodTable(vm)->getOwnPropertyNames(object, globalObject, keys, EnumerationMode(DontEnumPropertiesMode::Include));
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

    if (jsDynamicCast<JSFinalObject*>(vm, object) && !hasIndexedProperties(object->indexingType())) {
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

    if (jsDynamicCast<JSFinalObject*>(vm, object) && !hasIndexedProperties(object->indexingType())) {
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
    bool status = object->methodTable(vm)->preventExtensions(object, globalObject);
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
    if (jsDynamicCast<JSFinalObject*>(vm, object) && !hasIndexedProperties(object->indexingType()))
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
    if (jsDynamicCast<JSFinalObject*>(vm, object) && !hasIndexedProperties(object->indexingType()))
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

JSArray* ownPropertyKeys(JSGlobalObject* globalObject, JSObject* object, PropertyNameMode propertyNameMode, DontEnumPropertiesMode dontEnumPropertiesMode, Optional<CachedPropertyNamesKind> kind)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // We attempt to look up own property keys cache in Object.keys / Object.getOwnPropertyNames cases.
    if (kind) {
        if (LIKELY(!globalObject->isHavingABadTime())) {
            if (auto* immutableButterfly = object->structure(vm)->cachedPropertyNames(kind.value())) {
                Structure* arrayStructure = globalObject->originalArrayStructureForIndexingType(immutableButterfly->indexingMode());
                return JSArray::createWithButterfly(vm, nullptr, arrayStructure, immutableButterfly->toButterfly());
            }
        }
    }

    PropertyNameArray properties(vm, propertyNameMode, PrivateSymbolMode::Exclude);
    object->methodTable(vm)->getOwnPropertyNames(object, globalObject, properties, EnumerationMode(dontEnumPropertiesMode));
    RETURN_IF_EXCEPTION(scope, nullptr);

    if (propertyNameMode != PropertyNameMode::StringsAndSymbols) {
        ASSERT(propertyNameMode == PropertyNameMode::Strings || propertyNameMode == PropertyNameMode::Symbols);
        if (properties.size() < MIN_SPARSE_ARRAY_INDEX) {
            if (LIKELY(!globalObject->isHavingABadTime())) {
                if (kind) {
                    Structure* structure = object->structure(vm);
                    if (structure->canCacheOwnPropertyNames()) {
                        auto* cachedButterfly = structure->cachedPropertyNamesIgnoringSentinel(kind.value());
                        if (cachedButterfly == StructureRareData::cachedPropertyNamesSentinel()) {
                            size_t numProperties = properties.size();
                            auto* newButterfly = JSImmutableButterfly::create(vm, CopyOnWriteArrayWithContiguous, numProperties);
                            for (size_t i = 0; i < numProperties; i++) {
                                const auto& identifier = properties[i];
                                ASSERT(!identifier.isSymbol());
                                newButterfly->setIndex(vm, i, jsOwnedString(vm, identifier.string()));
                            }

                            structure->setCachedPropertyNames(vm, kind.value(), newButterfly);
                            Structure* arrayStructure = globalObject->originalArrayStructureForIndexingType(newButterfly->indexingMode());
                            return JSArray::createWithButterfly(vm, nullptr, arrayStructure, newButterfly->toButterfly());
                        }

                        if (cachedButterfly == nullptr)
                            structure->setCachedPropertyNames(vm, kind.value(), StructureRareData::cachedPropertyNamesSentinel());
                    }
                }

                size_t numProperties = properties.size();
                JSArray* keys = JSArray::create(vm, globalObject->originalArrayStructureForIndexingType(ArrayWithContiguous), numProperties);
                WriteBarrier<Unknown>* buffer = keys->butterfly()->contiguous().data();
                for (size_t i = 0; i < numProperties; i++) {
                    const auto& identifier = properties[i];
                    if (propertyNameMode == PropertyNameMode::Strings) {
                        ASSERT(!identifier.isSymbol());
                        buffer[i].set(vm, keys, jsOwnedString(vm, identifier.string()));
                    } else {
                        ASSERT(identifier.isSymbol());
                        buffer[i].set(vm, keys, Symbol::create(vm, static_cast<SymbolImpl&>(*identifier.impl())));
                    }
                }
                return keys;
            }
        }
    }

    JSArray* keys = constructEmptyArray(globalObject, nullptr);
    RETURN_IF_EXCEPTION(scope, nullptr);

    unsigned index = 0;
    auto pushDirect = [&] (JSGlobalObject* globalObject, JSArray* array, JSValue value) {
        array->putDirectIndex(globalObject, index++, value);
    };

    switch (propertyNameMode) {
    case PropertyNameMode::Strings: {
        size_t numProperties = properties.size();
        for (size_t i = 0; i < numProperties; i++) {
            const auto& identifier = properties[i];
            ASSERT(!identifier.isSymbol());
            pushDirect(globalObject, keys, jsOwnedString(vm, identifier.string()));
            RETURN_IF_EXCEPTION(scope, nullptr);
        }
        break;
    }

    case PropertyNameMode::Symbols: {
        size_t numProperties = properties.size();
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
        size_t numProperties = properties.size();
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

} // namespace JSC
