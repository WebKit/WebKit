/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2008-2017 Apple Inc. All rights reserved.
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
#include "ButterflyInlines.h"
#include "Error.h"
#include "ExceptionHelpers.h"
#include "JSArray.h"
#include "JSCInlines.h"
#include "JSFunction.h"
#include "JSGlobalObject.h"
#include "JSGlobalObjectFunctions.h"
#include "Lookup.h"
#include "ObjectPrototype.h"
#include "PropertyDescriptor.h"
#include "PropertyNameArray.h"
#include "StackVisitor.h"
#include "Symbol.h"

namespace JSC {

EncodedJSValue JSC_HOST_CALL objectConstructorAssign(ExecState*);
EncodedJSValue JSC_HOST_CALL objectConstructorValues(ExecState*);
EncodedJSValue JSC_HOST_CALL objectConstructorGetPrototypeOf(ExecState*);
EncodedJSValue JSC_HOST_CALL objectConstructorSetPrototypeOf(ExecState*);
EncodedJSValue JSC_HOST_CALL objectConstructorGetOwnPropertyNames(ExecState*);
EncodedJSValue JSC_HOST_CALL objectConstructorDefineProperty(ExecState*);
EncodedJSValue JSC_HOST_CALL objectConstructorDefineProperties(ExecState*);
EncodedJSValue JSC_HOST_CALL objectConstructorCreate(ExecState*);
EncodedJSValue JSC_HOST_CALL objectConstructorSeal(ExecState*);
EncodedJSValue JSC_HOST_CALL objectConstructorFreeze(ExecState*);
EncodedJSValue JSC_HOST_CALL objectConstructorPreventExtensions(ExecState*);
EncodedJSValue JSC_HOST_CALL objectConstructorIsSealed(ExecState*);
EncodedJSValue JSC_HOST_CALL objectConstructorIsFrozen(ExecState*);
EncodedJSValue JSC_HOST_CALL objectConstructorIsExtensible(ExecState*);
EncodedJSValue JSC_HOST_CALL objectConstructorIs(ExecState*);

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
  getOwnPropertyNames       objectConstructorGetOwnPropertyNames        DontEnum|Function 1
  getOwnPropertySymbols     objectConstructorGetOwnPropertySymbols      DontEnum|Function 1
  keys                      objectConstructorKeys                       DontEnum|Function 1
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


static EncodedJSValue JSC_HOST_CALL callObjectConstructor(ExecState*);
static EncodedJSValue JSC_HOST_CALL constructWithObjectConstructor(ExecState*);

ObjectConstructor::ObjectConstructor(VM& vm, Structure* structure)
    : InternalFunction(vm, structure, callObjectConstructor, constructWithObjectConstructor)
{
}

void ObjectConstructor::finishCreation(VM& vm, JSGlobalObject* globalObject, ObjectPrototype* objectPrototype)
{
    Base::finishCreation(vm, objectPrototype->classInfo(vm)->className);

    putDirectWithoutTransition(vm, vm.propertyNames->prototype, objectPrototype, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);
    putDirectWithoutTransition(vm, vm.propertyNames->length, jsNumber(1), PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum);

    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().createPrivateName(), objectConstructorCreate, static_cast<unsigned>(PropertyAttribute::DontEnum), 2);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().definePropertyPrivateName(), objectConstructorDefineProperty, static_cast<unsigned>(PropertyAttribute::DontEnum), 3);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().getPrototypeOfPrivateName(), objectConstructorGetPrototypeOf, static_cast<unsigned>(PropertyAttribute::DontEnum), 1);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().getOwnPropertyNamesPrivateName(), objectConstructorGetOwnPropertyNames, static_cast<unsigned>(PropertyAttribute::DontEnum), 1);
}

// ES 19.1.1.1 Object([value])
static ALWAYS_INLINE JSObject* constructObject(ExecState* exec, JSValue newTarget)
{
    VM& vm = exec->vm();
    ObjectConstructor* objectConstructor = jsCast<ObjectConstructor*>(exec->jsCallee());
    JSGlobalObject* globalObject = objectConstructor->globalObject(vm);
    auto scope = DECLARE_THROW_SCOPE(vm);

    // We need to check newTarget condition in this caller side instead of InternalFunction::createSubclassStructure side.
    // Since if we found this condition is met, we should not fall into the type conversion in the step 3.

    // 1. If NewTarget is neither undefined nor the active function, then
    if (newTarget && newTarget != objectConstructor) {
        // a. Return ? OrdinaryCreateFromConstructor(NewTarget, "%ObjectPrototype%").
        Structure* objectStructure = InternalFunction::createSubclassStructure(exec, newTarget, globalObject->objectStructureForObjectConstructor());
        RETURN_IF_EXCEPTION(scope, nullptr);
        return constructEmptyObject(exec, objectStructure);
    }

    // 2. If value is null, undefined or not supplied, return ObjectCreate(%ObjectPrototype%).
    ArgList args(exec);
    JSValue arg = args.at(0);
    if (arg.isUndefinedOrNull())
        return constructEmptyObject(exec, globalObject->objectStructureForObjectConstructor());

    // 3. Return ToObject(value).
    RELEASE_AND_RETURN(scope, arg.toObject(exec, globalObject));
}

static EncodedJSValue JSC_HOST_CALL constructWithObjectConstructor(ExecState* exec)
{
    return JSValue::encode(constructObject(exec, exec->newTarget()));
}

static EncodedJSValue JSC_HOST_CALL callObjectConstructor(ExecState* exec)
{
    return JSValue::encode(constructObject(exec, JSValue()));
}

EncodedJSValue JSC_HOST_CALL objectConstructorGetPrototypeOf(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSObject* object = exec->argument(0).toObject(exec);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    RELEASE_AND_RETURN(scope, JSValue::encode(object->getPrototype(vm, exec)));
}

EncodedJSValue JSC_HOST_CALL objectConstructorSetPrototypeOf(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue objectValue = exec->argument(0);
    if (objectValue.isUndefinedOrNull())
        return throwVMTypeError(exec, scope, "Cannot set prototype of undefined or null"_s);

    JSValue protoValue = exec->argument(1);
    if (!protoValue.isObject() && !protoValue.isNull())
        return throwVMTypeError(exec, scope, "Prototype value can only be an object or null"_s);

    JSObject* object = objectValue.toObject(exec);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    bool shouldThrowIfCantSet = true;
    bool didSetPrototype = object->setPrototype(vm, exec, protoValue, shouldThrowIfCantSet);
    EXCEPTION_ASSERT_UNUSED(didSetPrototype, scope.exception() || didSetPrototype);
    return JSValue::encode(objectValue);
}

JSValue objectConstructorGetOwnPropertyDescriptor(ExecState* exec, JSObject* object, const Identifier& propertyName)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    PropertyDescriptor descriptor;
    if (!object->getOwnPropertyDescriptor(exec, propertyName, descriptor))
        RELEASE_AND_RETURN(scope, jsUndefined());
    RETURN_IF_EXCEPTION(scope, { });

    JSObject* result = constructObjectFromPropertyDescriptor(exec, descriptor);
    EXCEPTION_ASSERT(!!scope.exception() == !result);
    if (!result)
        return jsUndefined();
    return result;
}

JSValue objectConstructorGetOwnPropertyDescriptors(ExecState* exec, JSObject* object)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    PropertyNameArray properties(&vm, PropertyNameMode::StringsAndSymbols, PrivateSymbolMode::Exclude);
    object->methodTable(vm)->getOwnPropertyNames(object, exec, properties, EnumerationMode(DontEnumPropertiesMode::Include));
    RETURN_IF_EXCEPTION(scope, { });

    JSObject* descriptors = constructEmptyObject(exec);
    RETURN_IF_EXCEPTION(scope, { });

    for (auto& propertyName : properties) {
        PropertyDescriptor descriptor;
        bool didGetDescriptor = object->getOwnPropertyDescriptor(exec, propertyName, descriptor);
        RETURN_IF_EXCEPTION(scope, { });

        if (!didGetDescriptor)
            continue;

        JSObject* fromDescriptor = constructObjectFromPropertyDescriptor(exec, descriptor);
        EXCEPTION_ASSERT(!!scope.exception() == !fromDescriptor);
        if (!fromDescriptor)
            return jsUndefined();

        PutPropertySlot slot(descriptors);
        descriptors->putOwnDataPropertyMayBeIndex(exec, propertyName, fromDescriptor, slot);
        scope.assertNoException();
    }

    return descriptors;
}

EncodedJSValue JSC_HOST_CALL objectConstructorGetOwnPropertyDescriptor(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSObject* object = exec->argument(0).toObject(exec);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    auto propertyName = exec->argument(1).toPropertyKey(exec);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    RELEASE_AND_RETURN(scope, JSValue::encode(objectConstructorGetOwnPropertyDescriptor(exec, object, propertyName)));
}

EncodedJSValue JSC_HOST_CALL objectConstructorGetOwnPropertyDescriptors(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSObject* object = exec->argument(0).toObject(exec);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    RELEASE_AND_RETURN(scope, JSValue::encode(objectConstructorGetOwnPropertyDescriptors(exec, object)));
}

// FIXME: Use the enumeration cache.
EncodedJSValue JSC_HOST_CALL objectConstructorGetOwnPropertyNames(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSObject* object = exec->argument(0).toObject(exec);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    RELEASE_AND_RETURN(scope, JSValue::encode(ownPropertyKeys(exec, object, PropertyNameMode::Strings, DontEnumPropertiesMode::Include)));
}

// FIXME: Use the enumeration cache.
EncodedJSValue JSC_HOST_CALL objectConstructorGetOwnPropertySymbols(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSObject* object = exec->argument(0).toObject(exec);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    RELEASE_AND_RETURN(scope, JSValue::encode(ownPropertyKeys(exec, object, PropertyNameMode::Symbols, DontEnumPropertiesMode::Include)));
}

// FIXME: Use the enumeration cache.
EncodedJSValue JSC_HOST_CALL objectConstructorKeys(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSObject* object = exec->argument(0).toObject(exec);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    RELEASE_AND_RETURN(scope, JSValue::encode(ownPropertyKeys(exec, object, PropertyNameMode::Strings, DontEnumPropertiesMode::Exclude)));
}

EncodedJSValue JSC_HOST_CALL objectConstructorAssign(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue targetValue = exec->argument(0);
    if (targetValue.isUndefinedOrNull())
        return throwVMTypeError(exec, scope, "Object.assign requires that input parameter not be null or undefined"_s);
    JSObject* target = targetValue.toObject(exec);
    RETURN_IF_EXCEPTION(scope, { });

    // FIXME: Extend this for non JSFinalObject. For example, we would like to use this fast path for function objects too.
    // https://bugs.webkit.org/show_bug.cgi?id=185358
    bool targetCanPerformFastPut = jsDynamicCast<JSFinalObject*>(vm, target) && target->canPerformFastPutInlineExcludingProto(vm);

    Vector<RefPtr<UniquedStringImpl>, 8> properties;
    MarkedArgumentBuffer values;
    unsigned argsCount = exec->argumentCount();
    for (unsigned i = 1; i < argsCount; ++i) {
        JSValue sourceValue = exec->uncheckedArgument(i);
        if (sourceValue.isUndefinedOrNull())
            continue;
        JSObject* source = sourceValue.toObject(exec);
        RETURN_IF_EXCEPTION(scope, { });

        if (targetCanPerformFastPut) {
            if (!source->staticPropertiesReified(vm)) {
                source->reifyAllStaticProperties(exec);
                RETURN_IF_EXCEPTION(scope, { });
            }

            auto canPerformFastPropertyEnumerationForObjectAssign = [] (Structure* structure) {
                if (structure->typeInfo().overridesGetOwnPropertySlot())
                    return false;
                if (structure->typeInfo().overridesGetPropertyNames())
                    return false;
                // FIXME: Indexed properties can be handled.
                // https://bugs.webkit.org/show_bug.cgi?id=185358
                if (hasIndexedProperties(structure->indexingType()))
                    return false;
                if (structure->hasGetterSetterProperties())
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

        PropertyNameArray properties(&vm, PropertyNameMode::StringsAndSymbols, PrivateSymbolMode::Exclude);
        source->methodTable(vm)->getOwnPropertyNames(source, exec, properties, EnumerationMode(DontEnumPropertiesMode::Include));
        RETURN_IF_EXCEPTION(scope, { });

        auto assign = [&] (PropertyName propertyName) {
            PropertySlot slot(source, PropertySlot::InternalMethodType::GetOwnProperty);
            bool hasProperty = source->methodTable(vm)->getOwnPropertySlot(source, exec, propertyName, slot);
            RETURN_IF_EXCEPTION(scope, void());
            if (!hasProperty)
                return;
            if (slot.attributes() & PropertyAttribute::DontEnum)
                return;

            JSValue value;
            if (LIKELY(!slot.isTaintedByOpaqueObject()))
                value = slot.getValue(exec, propertyName);
            else
                value = source->get(exec, propertyName);
            RETURN_IF_EXCEPTION(scope, void());

            PutPropertySlot putPropertySlot(target, true);
            target->putInline(exec, propertyName, value, putPropertySlot);
        };

        // First loop is for strings. Second loop is for symbols to keep standardized order requirement in the spec.
        // https://tc39.github.io/ecma262/#sec-ordinaryownpropertykeys
        bool foundSymbol = false;
        unsigned numProperties = properties.size();
        for (unsigned j = 0; j < numProperties; j++) {
            const auto& propertyName = properties[j];
            if (propertyName.isSymbol()) {
                foundSymbol = true;
                continue;
            }

            assign(propertyName);
            RETURN_IF_EXCEPTION(scope, { });
        }

        if (foundSymbol) {
            for (unsigned j = 0; j < numProperties; j++) {
                const auto& propertyName = properties[j];
                if (propertyName.isSymbol()) {
                    ASSERT(!propertyName.isPrivateName());
                    assign(propertyName);
                    RETURN_IF_EXCEPTION(scope, { });
                }
            }
        }
    }
    return JSValue::encode(target);
}

EncodedJSValue JSC_HOST_CALL objectConstructorValues(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue targetValue = exec->argument(0);
    if (targetValue.isUndefinedOrNull())
        return throwVMTypeError(exec, scope, "Object.values requires that input parameter not be null or undefined"_s);
    JSObject* target = targetValue.toObject(exec);
    RETURN_IF_EXCEPTION(scope, { });

    JSArray* values = constructEmptyArray(exec, nullptr);
    RETURN_IF_EXCEPTION(scope, { });

    PropertyNameArray properties(&vm, PropertyNameMode::Strings, PrivateSymbolMode::Exclude);
    target->methodTable(vm)->getOwnPropertyNames(target, exec, properties, EnumerationMode(DontEnumPropertiesMode::Include));
    RETURN_IF_EXCEPTION(scope, { });

    unsigned index = 0;
    auto addValue = [&] (PropertyName propertyName) {
        PropertySlot slot(target, PropertySlot::InternalMethodType::GetOwnProperty);
        bool hasProperty = target->methodTable(vm)->getOwnPropertySlot(target, exec, propertyName, slot);
        RETURN_IF_EXCEPTION(scope, void());
        if (!hasProperty)
            return;
        if (slot.attributes() & PropertyAttribute::DontEnum)
            return;

        JSValue value;
        if (LIKELY(!slot.isTaintedByOpaqueObject()))
            value = slot.getValue(exec, propertyName);
        else
            value = target->get(exec, propertyName);
        RETURN_IF_EXCEPTION(scope, void());

        values->putDirectIndex(exec, index++, value);
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
bool toPropertyDescriptor(ExecState* exec, JSValue in, PropertyDescriptor& desc)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!in.isObject()) {
        throwTypeError(exec, scope, "Property description must be an object."_s);
        return false;
    }
    JSObject* description = asObject(in);

    bool hasProperty = description->hasProperty(exec, vm.propertyNames->enumerable);
    EXCEPTION_ASSERT(!scope.exception() || !hasProperty);
    if (hasProperty) {
        JSValue value = description->get(exec, vm.propertyNames->enumerable);
        RETURN_IF_EXCEPTION(scope, false);
        desc.setEnumerable(value.toBoolean(exec));
    } else
        RETURN_IF_EXCEPTION(scope, false);

    hasProperty = description->hasProperty(exec, vm.propertyNames->configurable);
    EXCEPTION_ASSERT(!scope.exception() || !hasProperty);
    if (hasProperty) {
        JSValue value = description->get(exec, vm.propertyNames->configurable);
        RETURN_IF_EXCEPTION(scope, false);
        desc.setConfigurable(value.toBoolean(exec));
    } else
        RETURN_IF_EXCEPTION(scope, false);

    JSValue value;
    hasProperty = description->hasProperty(exec, vm.propertyNames->value);
    EXCEPTION_ASSERT(!scope.exception() || !hasProperty);
    if (hasProperty) {
        JSValue value = description->get(exec, vm.propertyNames->value);
        RETURN_IF_EXCEPTION(scope, false);
        desc.setValue(value);
    } else
        RETURN_IF_EXCEPTION(scope, false);

    hasProperty = description->hasProperty(exec, vm.propertyNames->writable);
    EXCEPTION_ASSERT(!scope.exception() || !hasProperty);
    if (hasProperty) {
        JSValue value = description->get(exec, vm.propertyNames->writable);
        RETURN_IF_EXCEPTION(scope, false);
        desc.setWritable(value.toBoolean(exec));
    } else
        RETURN_IF_EXCEPTION(scope, false);

    hasProperty = description->hasProperty(exec, vm.propertyNames->get);
    EXCEPTION_ASSERT(!scope.exception() || !hasProperty);
    if (hasProperty) {
        JSValue get = description->get(exec, vm.propertyNames->get);
        RETURN_IF_EXCEPTION(scope, false);
        if (!get.isUndefined()) {
            CallData callData;
            if (getCallData(vm, get, callData) == CallType::None) {
                throwTypeError(exec, scope, "Getter must be a function."_s);
                return false;
            }
        }
        desc.setGetter(get);
    } else
        RETURN_IF_EXCEPTION(scope, false);

    hasProperty = description->hasProperty(exec, vm.propertyNames->set);
    EXCEPTION_ASSERT(!scope.exception() || !hasProperty);
    if (hasProperty) {
        JSValue set = description->get(exec, vm.propertyNames->set);
        RETURN_IF_EXCEPTION(scope, false);
        if (!set.isUndefined()) {
            CallData callData;
            if (getCallData(vm, set, callData) == CallType::None) {
                throwTypeError(exec, scope, "Setter must be a function."_s);
                return false;
            }
        }
        desc.setSetter(set);
    } else
        RETURN_IF_EXCEPTION(scope, false);

    if (!desc.isAccessorDescriptor())
        return true;

    if (desc.value()) {
        throwTypeError(exec, scope, "Invalid property.  'value' present on property with getter or setter."_s);
        return false;
    }

    if (desc.writablePresent()) {
        throwTypeError(exec, scope, "Invalid property.  'writable' present on property with getter or setter."_s);
        return false;
    }
    return true;
}

EncodedJSValue JSC_HOST_CALL objectConstructorDefineProperty(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!exec->argument(0).isObject())
        return throwVMTypeError(exec, scope, "Properties can only be defined on Objects."_s);
    JSObject* obj = asObject(exec->argument(0));
    auto propertyName = exec->argument(1).toPropertyKey(exec);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    PropertyDescriptor descriptor;
    auto success = toPropertyDescriptor(exec, exec->argument(2), descriptor);
    EXCEPTION_ASSERT(!scope.exception() == success);
    if (!success)
        return JSValue::encode(jsNull());
    ASSERT((descriptor.attributes() & PropertyAttribute::Accessor) || (!descriptor.isAccessorDescriptor()));
    scope.assertNoException();
    obj->methodTable(vm)->defineOwnProperty(obj, exec, propertyName, descriptor, true);
    RELEASE_AND_RETURN(scope, JSValue::encode(obj));
}

static JSValue defineProperties(ExecState* exec, JSObject* object, JSObject* properties)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    PropertyNameArray propertyNames(&vm, PropertyNameMode::StringsAndSymbols, PrivateSymbolMode::Exclude);
    asObject(properties)->methodTable(vm)->getOwnPropertyNames(asObject(properties), exec, propertyNames, EnumerationMode(DontEnumPropertiesMode::Exclude));
    RETURN_IF_EXCEPTION(scope, { });
    size_t numProperties = propertyNames.size();
    Vector<PropertyDescriptor> descriptors;
    MarkedArgumentBuffer markBuffer;
    for (size_t i = 0; i < numProperties; i++) {
        JSValue prop = properties->get(exec, propertyNames[i]);
        RETURN_IF_EXCEPTION(scope, { });
        PropertyDescriptor descriptor;
        bool success = toPropertyDescriptor(exec, prop, descriptor);
        EXCEPTION_ASSERT(!scope.exception() || !success);
        if (UNLIKELY(!success)) {
            markBuffer.overflowCheckNotNeeded();
            return jsNull();
        }
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
    for (size_t i = 0; i < numProperties; i++) {
        auto& propertyName = propertyNames[i];
        ASSERT(!propertyName.isPrivateName());

        object->methodTable(vm)->defineOwnProperty(object, exec, propertyName, descriptors[i], true);
        RETURN_IF_EXCEPTION(scope, { });
    }
    return object;
}

EncodedJSValue JSC_HOST_CALL objectConstructorDefineProperties(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!exec->argument(0).isObject())
        return throwVMTypeError(exec, scope, "Properties can only be defined on Objects."_s);
    JSObject* targetObj = asObject(exec->argument(0));
    JSObject* props = exec->argument(1).toObject(exec);
    EXCEPTION_ASSERT(!!scope.exception() == !props);
    if (UNLIKELY(!props))
        return encodedJSValue();
    RELEASE_AND_RETURN(scope, JSValue::encode(defineProperties(exec, targetObj, props)));
}

EncodedJSValue JSC_HOST_CALL objectConstructorCreate(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue proto = exec->argument(0);
    if (!proto.isObject() && !proto.isNull())
        return throwVMTypeError(exec, scope, "Object prototype may only be an Object or null."_s);
    JSObject* newObject = proto.isObject()
        ? constructEmptyObject(exec, asObject(proto))
        : constructEmptyObject(exec, exec->lexicalGlobalObject()->nullPrototypeObjectStructure());
    if (exec->argument(1).isUndefined())
        return JSValue::encode(newObject);
    if (!exec->argument(1).isObject())
        return throwVMTypeError(exec, scope, "Property descriptor list must be an Object."_s);
    RELEASE_AND_RETURN(scope, JSValue::encode(defineProperties(exec, newObject, asObject(exec->argument(1)))));
}

enum class IntegrityLevel {
    Sealed,
    Frozen
};

template<IntegrityLevel level>
bool setIntegrityLevel(ExecState* exec, VM& vm, JSObject* object)
{
    // See https://tc39.github.io/ecma262/#sec-setintegritylevel.
    auto scope = DECLARE_THROW_SCOPE(vm);

    bool success = object->methodTable(vm)->preventExtensions(object, exec);
    RETURN_IF_EXCEPTION(scope, false);
    if (UNLIKELY(!success))
        return false;

    PropertyNameArray properties(&vm, PropertyNameMode::StringsAndSymbols, PrivateSymbolMode::Exclude);
    object->methodTable(vm)->getOwnPropertyNames(object, exec, properties, EnumerationMode(DontEnumPropertiesMode::Include));
    RETURN_IF_EXCEPTION(scope, false);

    PropertyNameArray::const_iterator end = properties.end();
    for (PropertyNameArray::const_iterator iter = properties.begin(); iter != end; ++iter) {
        auto& propertyName = *iter;
        ASSERT(!propertyName.isPrivateName());

        PropertyDescriptor desc;
        if (level == IntegrityLevel::Sealed)
            desc.setConfigurable(false);
        else {
            bool hasPropertyDescriptor = object->getOwnPropertyDescriptor(exec, propertyName, desc);
            RETURN_IF_EXCEPTION(scope, false);
            if (!hasPropertyDescriptor)
                continue;

            if (desc.isDataDescriptor())
                desc.setWritable(false);

            desc.setConfigurable(false);
        }

        object->methodTable(vm)->defineOwnProperty(object, exec, propertyName, desc, true);
        RETURN_IF_EXCEPTION(scope, false);
    }
    return true;
}

template<IntegrityLevel level>
bool testIntegrityLevel(ExecState* exec, VM& vm, JSObject* object)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 1. Assert: Type(O) is Object.
    // 2. Assert: level is either "sealed" or "frozen".

    // 3. Let status be ?IsExtensible(O).
    bool status = object->isExtensible(exec);
    RETURN_IF_EXCEPTION(scope, { });

    // 4. If status is true, return false.
    if (status)
        return false;

    // 6. Let keys be ? O.[[OwnPropertyKeys]]().
    PropertyNameArray keys(&vm, PropertyNameMode::StringsAndSymbols, PrivateSymbolMode::Exclude);
    object->methodTable(vm)->getOwnPropertyNames(object, exec, keys, EnumerationMode(DontEnumPropertiesMode::Include));
    RETURN_IF_EXCEPTION(scope, { });

    // 7. For each element k of keys, do
    PropertyNameArray::const_iterator end = keys.end();
    for (PropertyNameArray::const_iterator iter = keys.begin(); iter != end; ++iter) {
        auto& propertyName = *iter;
        ASSERT(!propertyName.isPrivateName());

        // a. Let currentDesc be ? O.[[GetOwnProperty]](k)
        PropertyDescriptor desc;
        bool didGetDescriptor = object->getOwnPropertyDescriptor(exec, propertyName, desc);
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

JSObject* objectConstructorSeal(ExecState* exec, JSObject* object)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (jsDynamicCast<JSFinalObject*>(vm, object) && !hasIndexedProperties(object->indexingType())) {
        object->seal(vm);
        return object;
    }

    bool success = setIntegrityLevel<IntegrityLevel::Sealed>(exec, vm, object);
    RETURN_IF_EXCEPTION(scope, nullptr);
    if (UNLIKELY(!success))
        return throwTypeError(exec, scope, "Unable to prevent extension in Object.seal"_s);

    return object;
}

EncodedJSValue JSC_HOST_CALL objectConstructorSeal(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 1. If Type(O) is not Object, return O.
    JSValue obj = exec->argument(0);
    if (!obj.isObject())
        return JSValue::encode(obj);

    RELEASE_AND_RETURN(scope, JSValue::encode(objectConstructorSeal(exec, asObject(obj))));
}

JSObject* objectConstructorFreeze(ExecState* exec, JSObject* object)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (jsDynamicCast<JSFinalObject*>(vm, object) && !hasIndexedProperties(object->indexingType())) {
        object->freeze(vm);
        return object;
    }

    bool success = setIntegrityLevel<IntegrityLevel::Frozen>(exec, vm, object);
    RETURN_IF_EXCEPTION(scope, nullptr);
    if (!success)
        return throwTypeError(exec, scope, "Unable to prevent extension in Object.freeze"_s);
    return object;
}

EncodedJSValue JSC_HOST_CALL objectConstructorFreeze(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    // 1. If Type(O) is not Object, return O.
    JSValue obj = exec->argument(0);
    if (!obj.isObject())
        return JSValue::encode(obj);
    JSObject* result = objectConstructorFreeze(exec, asObject(obj));
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    return JSValue::encode(result);
}

EncodedJSValue JSC_HOST_CALL objectConstructorPreventExtensions(ExecState* exec)
{
    VM& vm = exec->vm();
    JSValue argument = exec->argument(0);
    if (!argument.isObject())
        return JSValue::encode(argument);
    JSObject* object = asObject(argument);
    object->methodTable(vm)->preventExtensions(object, exec);
    return JSValue::encode(object);
}

EncodedJSValue JSC_HOST_CALL objectConstructorIsSealed(ExecState* exec)
{
    VM& vm = exec->vm();

    // 1. If Type(O) is not Object, return true.
    JSValue obj = exec->argument(0);
    if (!obj.isObject())
        return JSValue::encode(jsBoolean(true));
    JSObject* object = asObject(obj);

    // Quick check for final objects.
    if (jsDynamicCast<JSFinalObject*>(vm, object) && !hasIndexedProperties(object->indexingType()))
        return JSValue::encode(jsBoolean(object->isSealed(vm)));

    // 2. Return ? TestIntegrityLevel(O, "sealed").
    return JSValue::encode(jsBoolean(testIntegrityLevel<IntegrityLevel::Sealed>(exec, vm, object)));
}

EncodedJSValue JSC_HOST_CALL objectConstructorIsFrozen(ExecState* exec)
{
    VM& vm = exec->vm();

    // 1. If Type(O) is not Object, return true.
    JSValue obj = exec->argument(0);
    if (!obj.isObject())
        return JSValue::encode(jsBoolean(true));
    JSObject* object = asObject(obj);

    // Quick check for final objects.
    if (jsDynamicCast<JSFinalObject*>(vm, object) && !hasIndexedProperties(object->indexingType()))
        return JSValue::encode(jsBoolean(object->isFrozen(vm)));

    // 2. Return ? TestIntegrityLevel(O, "frozen").
    return JSValue::encode(jsBoolean(testIntegrityLevel<IntegrityLevel::Frozen>(exec, vm, object)));
}

EncodedJSValue JSC_HOST_CALL objectConstructorIsExtensible(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue obj = exec->argument(0);
    if (!obj.isObject())
        return JSValue::encode(jsBoolean(false));
    JSObject* object = asObject(obj);
    bool isExtensible = object->isExtensible(exec);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    return JSValue::encode(jsBoolean(isExtensible));
}

EncodedJSValue JSC_HOST_CALL objectConstructorIs(ExecState* exec)
{
    return JSValue::encode(jsBoolean(sameValue(exec, exec->argument(0), exec->argument(1))));
}

// FIXME: Use the enumeration cache.
JSArray* ownPropertyKeys(ExecState* exec, JSObject* object, PropertyNameMode propertyNameMode, DontEnumPropertiesMode dontEnumPropertiesMode)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    PropertyNameArray properties(&vm, propertyNameMode, PrivateSymbolMode::Exclude);
    object->methodTable(vm)->getOwnPropertyNames(object, exec, properties, EnumerationMode(dontEnumPropertiesMode));
    RETURN_IF_EXCEPTION(scope, nullptr);

    // https://tc39.github.io/ecma262/#sec-enumerableownproperties
    // If {object} is a Proxy, an explicit and observable [[GetOwnProperty]] op is required to filter out non-enumerable properties.
    // In other cases, filtering has already been performed.
    const bool mustFilterProperty = dontEnumPropertiesMode == DontEnumPropertiesMode::Exclude && object->type() == ProxyObjectType;
    auto filterPropertyIfNeeded = [exec, object, mustFilterProperty](const Identifier& identifier) {
        if (!mustFilterProperty)
            return true;
        PropertyDescriptor descriptor;
        PropertyName name(identifier);
        return object->getOwnPropertyDescriptor(exec, name, descriptor) && descriptor.enumerable();
    };

    // If !mustFilterProperty and PropertyNameMode::Strings mode, we do not need to filter out any entries in PropertyNameArray.
    // We can use fast allocation and initialization.
    if (propertyNameMode != PropertyNameMode::StringsAndSymbols) {
        ASSERT(propertyNameMode == PropertyNameMode::Strings || propertyNameMode == PropertyNameMode::Symbols);
        if (!mustFilterProperty && properties.size() < MIN_SPARSE_ARRAY_INDEX) {
            auto* globalObject = exec->lexicalGlobalObject();
            if (LIKELY(!globalObject->isHavingABadTime())) {
                size_t numProperties = properties.size();
                JSArray* keys = JSArray::create(vm, globalObject->originalArrayStructureForIndexingType(ArrayWithContiguous), numProperties);
                WriteBarrier<Unknown>* buffer = keys->butterfly()->contiguous().data();
                for (size_t i = 0; i < numProperties; i++) {
                    const auto& identifier = properties[i];
                    if (propertyNameMode == PropertyNameMode::Strings) {
                        ASSERT(!identifier.isSymbol());
                        buffer[i].set(vm, keys, jsOwnedString(&vm, identifier.string()));
                    } else {
                        ASSERT(identifier.isSymbol());
                        buffer[i].set(vm, keys, Symbol::create(vm, static_cast<SymbolImpl&>(*identifier.impl())));
                    }
                }
                return keys;
            }
        }
    }

    JSArray* keys = constructEmptyArray(exec, nullptr);
    RETURN_IF_EXCEPTION(scope, nullptr);

    unsigned index = 0;
    auto pushDirect = [&] (ExecState* exec, JSArray* array, JSValue value) {
        array->putDirectIndex(exec, index++, value);
    };

    switch (propertyNameMode) {
    case PropertyNameMode::Strings: {
        size_t numProperties = properties.size();
        for (size_t i = 0; i < numProperties; i++) {
            const auto& identifier = properties[i];
            ASSERT(!identifier.isSymbol());
            bool hasProperty = filterPropertyIfNeeded(identifier);
            EXCEPTION_ASSERT(!scope.exception() || !hasProperty);
            if (hasProperty)
                pushDirect(exec, keys, jsOwnedString(exec, identifier.string()));
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
            bool hasProperty = filterPropertyIfNeeded(identifier);
            EXCEPTION_ASSERT(!scope.exception() || !hasProperty);
            if (hasProperty)
                pushDirect(exec, keys, Symbol::create(vm, static_cast<SymbolImpl&>(*identifier.impl())));
            RETURN_IF_EXCEPTION(scope, nullptr);
        }
        break;
    }

    case PropertyNameMode::StringsAndSymbols: {
        Vector<Identifier, 16> propertySymbols;
        size_t numProperties = properties.size();
        for (size_t i = 0; i < numProperties; i++) {
            const auto& identifier = properties[i];
            if (identifier.isSymbol()) {
                ASSERT(!identifier.isPrivateName());
                propertySymbols.append(identifier);
                continue;
            }

            bool hasProperty = filterPropertyIfNeeded(identifier);
            EXCEPTION_ASSERT(!scope.exception() || !hasProperty);
            if (hasProperty)
                pushDirect(exec, keys, jsOwnedString(exec, identifier.string()));
            RETURN_IF_EXCEPTION(scope, nullptr);
        }

        // To ensure the order defined in the spec (9.1.12), we append symbols at the last elements of keys.
        for (const auto& identifier : propertySymbols) {
            bool hasProperty = filterPropertyIfNeeded(identifier);
            EXCEPTION_ASSERT(!scope.exception() || !hasProperty);
            if (hasProperty)
                pushDirect(exec, keys, Symbol::create(vm, static_cast<SymbolImpl&>(*identifier.impl())));
            RETURN_IF_EXCEPTION(scope, nullptr);
        }

        break;
    }
    }

    return keys;
}

} // namespace JSC
