/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2008 Apple Inc. All rights reserved.
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

#include "ButterflyInlines.h"
#include "CopiedSpaceInlines.h"
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

const ClassInfo ObjectConstructor::s_info = { "Function", &InternalFunction::s_info, &objectConstructorTable, CREATE_METHOD_TABLE(ObjectConstructor) };

/* Source for ObjectConstructor.lut.h
@begin objectConstructorTable
  getPrototypeOf            objectConstructorGetPrototypeOf             DontEnum|Function 1
  setPrototypeOf            objectConstructorSetPrototypeOf             DontEnum|Function 2
  getOwnPropertyDescriptor  objectConstructorGetOwnPropertyDescriptor   DontEnum|Function 2
  getOwnPropertyDescriptors objectConstructorGetOwnPropertyDescriptors  DontEnum|Function 1
  getOwnPropertyNames       objectConstructorGetOwnPropertyNames        DontEnum|Function 1
  getOwnPropertySymbols     objectConstructorGetOwnPropertySymbols      DontEnum|Function 1
  keys                      objectConstructorKeys                       DontEnum|Function 1
  defineProperty            objectConstructorDefineProperty             DontEnum|Function 3
  defineProperties          objectConstructorDefineProperties           DontEnum|Function 2
  create                    objectConstructorCreate                     DontEnum|Function 2
  seal                      objectConstructorSeal                       DontEnum|Function 1
  freeze                    objectConstructorFreeze                     DontEnum|Function 1
  preventExtensions         objectConstructorPreventExtensions          DontEnum|Function 1
  isSealed                  objectConstructorIsSealed                   DontEnum|Function 1
  isFrozen                  objectConstructorIsFrozen                   DontEnum|Function 1
  isExtensible              objectConstructorIsExtensible               DontEnum|Function 1
  is                        objectConstructorIs                         DontEnum|Function 2
  assign                    JSBuiltin                                   DontEnum|Function 2
@end
*/

ObjectConstructor::ObjectConstructor(VM& vm, Structure* structure)
    : InternalFunction(vm, structure)
{
}

void ObjectConstructor::finishCreation(VM& vm, JSGlobalObject* globalObject, ObjectPrototype* objectPrototype)
{
    Base::finishCreation(vm, objectPrototype->classInfo()->className);
    // ECMA 15.2.3.1
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, objectPrototype, DontEnum | DontDelete | ReadOnly);
    // no. of arguments for constructor
    putDirectWithoutTransition(vm, vm.propertyNames->length, jsNumber(1), ReadOnly | DontEnum | DontDelete);

    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->getPrototypeOfPrivateName, objectConstructorGetPrototypeOf, DontEnum, 1);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->getOwnPropertyNamesPrivateName, objectConstructorGetOwnPropertyNames, DontEnum, 1);
}

JSFunction* ObjectConstructor::addDefineProperty(ExecState* exec, JSGlobalObject* globalObject)
{
    VM& vm = exec->vm();
    JSFunction* definePropertyFunction = JSFunction::create(vm, globalObject, 3, vm.propertyNames->defineProperty.string(), objectConstructorDefineProperty);
    putDirectWithoutTransition(vm, vm.propertyNames->defineProperty, definePropertyFunction, DontEnum);
    return definePropertyFunction;
}

bool ObjectConstructor::getOwnPropertySlot(JSObject* object, ExecState* exec, PropertyName propertyName, PropertySlot &slot)
{
    return getStaticFunctionSlot<JSObject>(exec, objectConstructorTable, jsCast<ObjectConstructor*>(object), propertyName, slot);
}

static ALWAYS_INLINE JSObject* constructObject(ExecState* exec)
{
    JSGlobalObject* globalObject = exec->callee()->globalObject();
    ArgList args(exec);
    JSValue arg = args.at(0);
    if (arg.isUndefinedOrNull())
        return constructEmptyObject(exec, globalObject->objectPrototype());
    return arg.toObject(exec, globalObject);
}

static EncodedJSValue JSC_HOST_CALL constructWithObjectConstructor(ExecState* exec)
{
    return JSValue::encode(constructObject(exec));
}

ConstructType ObjectConstructor::getConstructData(JSCell*, ConstructData& constructData)
{
    constructData.native.function = constructWithObjectConstructor;
    return ConstructType::Host;
}

static EncodedJSValue JSC_HOST_CALL callObjectConstructor(ExecState* exec)
{
    return JSValue::encode(constructObject(exec));
}

CallType ObjectConstructor::getCallData(JSCell*, CallData& callData)
{
    callData.native.function = callObjectConstructor;
    return CallType::Host;
}

class ObjectConstructorGetPrototypeOfFunctor {
public:
    ObjectConstructorGetPrototypeOfFunctor(ExecState* exec, JSObject* object)
        : m_exec(exec)
        , m_hasSkippedFirstFrame(false)
        , m_object(object)
        , m_result(jsUndefined())
    {
    }

    JSValue result() const { return m_result; }

    StackVisitor::Status operator()(StackVisitor& visitor)
    {
        if (!m_hasSkippedFirstFrame) {
            m_hasSkippedFirstFrame = true;
            return StackVisitor::Continue;
        }

        if (m_object->allowsAccessFrom(visitor->callFrame()))
            m_result = m_object->getPrototype(m_exec->vm(), m_exec);
        return StackVisitor::Done;
    }

private:
    ExecState* m_exec;
    bool m_hasSkippedFirstFrame;
    JSObject* m_object;
    JSValue m_result;
};

JSValue objectConstructorGetPrototypeOf(ExecState* exec, JSObject* object)
{
    ObjectConstructorGetPrototypeOfFunctor functor(exec, object);
    // This can throw but it's just unneeded extra work to check for it. The return
    // value from this function is only used as the return value from a host call.
    // Therefore, the return value is only used if there wasn't an exception.
    exec->iterate(functor);
    return functor.result();
}

EncodedJSValue JSC_HOST_CALL objectConstructorGetPrototypeOf(ExecState* exec)
{
    JSObject* object = exec->argument(0).toObject(exec);
    if (exec->hadException())
        return JSValue::encode(jsUndefined());
    return JSValue::encode(objectConstructorGetPrototypeOf(exec, object));
}

EncodedJSValue JSC_HOST_CALL objectConstructorSetPrototypeOf(ExecState* exec)
{
    JSValue objectValue = exec->argument(0);
    if (objectValue.isUndefinedOrNull())
        return throwVMTypeError(exec);

    JSValue protoValue = exec->argument(1);
    if (!protoValue.isObject() && !protoValue.isNull())
        return throwVMTypeError(exec);

    JSObject* object = objectValue.toObject(exec);
    if (exec->hadException())
        return JSValue::encode(objectValue);

    if (!checkProtoSetterAccessAllowed(exec, object))
        return JSValue::encode(objectValue);

    VM& vm = exec->vm();
    bool shouldThrowIfCantSet = true;
    bool didSetPrototype = object->setPrototype(vm, exec, protoValue, shouldThrowIfCantSet);
    ASSERT_UNUSED(didSetPrototype, vm.exception() || didSetPrototype);
    return JSValue::encode(objectValue);
}

JSValue objectConstructorGetOwnPropertyDescriptor(ExecState* exec, JSObject* object, const Identifier& propertyName)
{
    PropertyDescriptor descriptor;
    if (!object->getOwnPropertyDescriptor(exec, propertyName, descriptor))
        return jsUndefined();
    if (exec->hadException())
        return jsUndefined();

    JSObject* result = constructObjectFromPropertyDescriptor(exec, descriptor);
    if (!result)
        return jsUndefined();
    return result;
}

JSValue objectConstructorGetOwnPropertyDescriptors(ExecState* exec, JSObject* object)
{
    PropertyNameArray properties(exec, PropertyNameMode::StringsAndSymbols);
    object->methodTable(exec->vm())->getOwnPropertyNames(object, exec, properties, EnumerationMode(DontEnumPropertiesMode::Include));
    if (exec->hadException())
        return jsUndefined();

    JSObject* descriptors = constructEmptyObject(exec);
    if (exec->hadException())
        return jsUndefined();

    for (auto& propertyName : properties) {
        JSValue fromDescriptor = objectConstructorGetOwnPropertyDescriptor(exec, object, propertyName);
        if (exec->hadException())
            return jsUndefined();

        PutPropertySlot slot(descriptors);
        descriptors->putOwnDataPropertyMayBeIndex(exec, propertyName, fromDescriptor, slot);
        ASSERT(!exec->hadException());
    }

    return descriptors;
}

EncodedJSValue JSC_HOST_CALL objectConstructorGetOwnPropertyDescriptor(ExecState* exec)
{
    JSObject* object = exec->argument(0).toObject(exec);
    if (exec->hadException())
        return JSValue::encode(jsUndefined());
    auto propertyName = exec->argument(1).toPropertyKey(exec);
    if (exec->hadException())
        return JSValue::encode(jsUndefined());
    return JSValue::encode(objectConstructorGetOwnPropertyDescriptor(exec, object, propertyName));
}

EncodedJSValue JSC_HOST_CALL objectConstructorGetOwnPropertyDescriptors(ExecState* exec)
{
    JSObject* object = exec->argument(0).toObject(exec);
    if (exec->hadException())
        return JSValue::encode(jsUndefined());
    return JSValue::encode(objectConstructorGetOwnPropertyDescriptors(exec, object));
}

// FIXME: Use the enumeration cache.
EncodedJSValue JSC_HOST_CALL objectConstructorGetOwnPropertyNames(ExecState* exec)
{
    JSObject* object = exec->argument(0).toObject(exec);
    if (exec->hadException())
        return JSValue::encode(jsNull());
    return JSValue::encode(ownPropertyKeys(exec, object, PropertyNameMode::Strings, DontEnumPropertiesMode::Include));
}

// FIXME: Use the enumeration cache.
EncodedJSValue JSC_HOST_CALL objectConstructorGetOwnPropertySymbols(ExecState* exec)
{
    JSObject* object = exec->argument(0).toObject(exec);
    if (exec->hadException())
        return JSValue::encode(jsNull());
    return JSValue::encode(ownPropertyKeys(exec, object, PropertyNameMode::Symbols, DontEnumPropertiesMode::Include));
}

// FIXME: Use the enumeration cache.
EncodedJSValue JSC_HOST_CALL objectConstructorKeys(ExecState* exec)
{
    JSObject* object = exec->argument(0).toObject(exec);
    if (exec->hadException())
        return JSValue::encode(jsNull());
    return JSValue::encode(ownPropertyKeys(exec, object, PropertyNameMode::Strings, DontEnumPropertiesMode::Exclude));
}

EncodedJSValue JSC_HOST_CALL ownEnumerablePropertyKeys(ExecState* exec)
{
    JSObject* object = exec->argument(0).toObject(exec);
    if (exec->hadException())
        return JSValue::encode(jsNull());
    return JSValue::encode(ownPropertyKeys(exec, object, PropertyNameMode::StringsAndSymbols, DontEnumPropertiesMode::Exclude));
}

// ES6 6.2.4.5 ToPropertyDescriptor
// https://tc39.github.io/ecma262/#sec-topropertydescriptor
bool toPropertyDescriptor(ExecState* exec, JSValue in, PropertyDescriptor& desc)
{
    VM& vm = exec->vm();
    if (!in.isObject()) {
        vm.throwException(exec, createTypeError(exec, ASCIILiteral("Property description must be an object.")));
        return false;
    }
    JSObject* description = asObject(in);

    if (description->hasProperty(exec, exec->propertyNames().enumerable)) {
        JSValue value = description->get(exec, exec->propertyNames().enumerable);
        if (vm.exception())
            return false;
        desc.setEnumerable(value.toBoolean(exec));
    } else if (vm.exception())
        return false;

    if (description->hasProperty(exec, exec->propertyNames().configurable)) {
        JSValue value = description->get(exec, exec->propertyNames().configurable);
        if (vm.exception())
            return false;
        desc.setConfigurable(value.toBoolean(exec));
    } else if (vm.exception())
        return false;

    JSValue value;
    if (description->hasProperty(exec, exec->propertyNames().value)) {
        JSValue value = description->get(exec, exec->propertyNames().value);
        if (vm.exception())
            return false;
        desc.setValue(value);
    } else if (vm.exception())
        return false;

    if (description->hasProperty(exec, exec->propertyNames().writable)) {
        JSValue value = description->get(exec, exec->propertyNames().writable);
        if (vm.exception())
            return false;
        desc.setWritable(value.toBoolean(exec));
    } else if (vm.exception())
        return false;

    if (description->hasProperty(exec, exec->propertyNames().get)) {
        JSValue get = description->get(exec, exec->propertyNames().get);
        if (vm.exception())
            return false;
        if (!get.isUndefined()) {
            CallData callData;
            if (getCallData(get, callData) == CallType::None) {
                vm.throwException(exec, createTypeError(exec, ASCIILiteral("Getter must be a function.")));
                return false;
            }
        }
        desc.setGetter(get);
    } else if (vm.exception())
        return false;

    if (description->hasProperty(exec, exec->propertyNames().set)) {
        JSValue set = description->get(exec, exec->propertyNames().set);
        if (vm.exception())
            return false;
        if (!set.isUndefined()) {
            CallData callData;
            if (getCallData(set, callData) == CallType::None) {
                vm.throwException(exec, createTypeError(exec, ASCIILiteral("Setter must be a function.")));
                return false;
            }
        }
        desc.setSetter(set);
    } else if (vm.exception())
        return false;

    if (!desc.isAccessorDescriptor())
        return true;

    if (desc.value()) {
        vm.throwException(exec, createTypeError(exec, ASCIILiteral("Invalid property.  'value' present on property with getter or setter.")));
        return false;
    }

    if (desc.writablePresent()) {
        vm.throwException(exec, createTypeError(exec, ASCIILiteral("Invalid property.  'writable' present on property with getter or setter.")));
        return false;
    }
    return true;
}

EncodedJSValue JSC_HOST_CALL objectConstructorDefineProperty(ExecState* exec)
{
    if (!exec->argument(0).isObject())
        return throwVMError(exec, createTypeError(exec, ASCIILiteral("Properties can only be defined on Objects.")));
    JSObject* O = asObject(exec->argument(0));
    auto propertyName = exec->argument(1).toPropertyKey(exec);
    if (exec->hadException())
        return JSValue::encode(jsNull());
    PropertyDescriptor descriptor;
    if (!toPropertyDescriptor(exec, exec->argument(2), descriptor))
        return JSValue::encode(jsNull());
    ASSERT((descriptor.attributes() & Accessor) || (!descriptor.isAccessorDescriptor()));
    ASSERT(!exec->hadException());
    O->methodTable(exec->vm())->defineOwnProperty(O, exec, propertyName, descriptor, true);
    return JSValue::encode(O);
}

static JSValue defineProperties(ExecState* exec, JSObject* object, JSObject* properties)
{
    PropertyNameArray propertyNames(exec, PropertyNameMode::StringsAndSymbols);
    asObject(properties)->methodTable(exec->vm())->getOwnPropertyNames(asObject(properties), exec, propertyNames, EnumerationMode(DontEnumPropertiesMode::Exclude));
    if (UNLIKELY(exec->hadException()))
        return jsNull();
    size_t numProperties = propertyNames.size();
    Vector<PropertyDescriptor> descriptors;
    MarkedArgumentBuffer markBuffer;
    for (size_t i = 0; i < numProperties; i++) {
        JSValue prop = properties->get(exec, propertyNames[i]);
        if (exec->hadException())
            return jsNull();
        PropertyDescriptor descriptor;
        if (!toPropertyDescriptor(exec, prop, descriptor))
            return jsNull();
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
    for (size_t i = 0; i < numProperties; i++) {
        Identifier propertyName = propertyNames[i];
        if (exec->propertyNames().isPrivateName(propertyName))
            continue;
        object->methodTable(exec->vm())->defineOwnProperty(object, exec, propertyName, descriptors[i], true);
        if (exec->hadException())
            return jsNull();
    }
    return object;
}

EncodedJSValue JSC_HOST_CALL objectConstructorDefineProperties(ExecState* exec)
{
    if (!exec->argument(0).isObject())
        return throwVMError(exec, createTypeError(exec, ASCIILiteral("Properties can only be defined on Objects.")));
    JSObject* targetObj = asObject(exec->argument(0));
    JSObject* props = exec->argument(1).toObject(exec);
    if (!props)
        return JSValue::encode(JSValue());
    return JSValue::encode(defineProperties(exec, targetObj, props));
}

EncodedJSValue JSC_HOST_CALL objectConstructorCreate(ExecState* exec)
{
    JSValue proto = exec->argument(0);
    if (!proto.isObject() && !proto.isNull())
        return throwVMError(exec, createTypeError(exec, ASCIILiteral("Object prototype may only be an Object or null.")));
    JSObject* newObject = proto.isObject()
        ? constructEmptyObject(exec, asObject(proto))
        : constructEmptyObject(exec, exec->lexicalGlobalObject()->nullPrototypeObjectStructure());
    if (exec->argument(1).isUndefined())
        return JSValue::encode(newObject);
    if (!exec->argument(1).isObject())
        return throwVMError(exec, createTypeError(exec, ASCIILiteral("Property descriptor list must be an Object.")));
    return JSValue::encode(defineProperties(exec, newObject, asObject(exec->argument(1))));
}

EncodedJSValue JSC_HOST_CALL objectConstructorSeal(ExecState* exec)
{
    // 1. If Type(O) is not Object, return O.
    JSValue obj = exec->argument(0);
    if (!obj.isObject())
        return JSValue::encode(obj);
    JSObject* object = asObject(obj);

    if (isJSFinalObject(object)) {
        object->seal(exec->vm());
        return JSValue::encode(obj);
    }

    // 2. For each named own property name P of O,
    PropertyNameArray properties(exec, PropertyNameMode::StringsAndSymbols);
    object->methodTable(exec->vm())->getOwnPropertyNames(object, exec, properties, EnumerationMode(DontEnumPropertiesMode::Include));
    if (UNLIKELY(exec->hadException()))
        return JSValue::encode(obj);
    PropertyNameArray::const_iterator end = properties.end();
    for (PropertyNameArray::const_iterator iter = properties.begin(); iter != end; ++iter) {
        Identifier propertyName = *iter;
        if (exec->propertyNames().isPrivateName(propertyName))
            continue;
        // a. Let desc be the result of calling the [[GetOwnProperty]] internal method of O with P.
        PropertyDescriptor desc;
        if (!object->getOwnPropertyDescriptor(exec, propertyName, desc))
            continue;
        // b. If desc.[[Configurable]] is true, set desc.[[Configurable]] to false.
        desc.setConfigurable(false);
        // c. Call the [[DefineOwnProperty]] internal method of O with P, desc, and true as arguments.
        object->methodTable(exec->vm())->defineOwnProperty(object, exec, propertyName, desc, true);
        if (exec->hadException())
            return JSValue::encode(obj);
    }

    // 3. Set the [[Extensible]] internal property of O to false.
    object->methodTable(exec->vm())->preventExtensions(object, exec);
    if (exec->hadException())
        return JSValue::encode(JSValue());

    // 4. Return O.
    return JSValue::encode(obj);
}

JSObject* objectConstructorFreeze(ExecState* exec, JSObject* object)
{
    if (isJSFinalObject(object) && !hasIndexedProperties(object->indexingType())) {
        object->freeze(exec->vm());
        return object;
    }

    // 2. For each named own property name P of O,
    PropertyNameArray properties(exec, PropertyNameMode::StringsAndSymbols);
    object->methodTable(exec->vm())->getOwnPropertyNames(object, exec, properties, EnumerationMode(DontEnumPropertiesMode::Include));
    if (UNLIKELY(exec->hadException()))
        return object;
    PropertyNameArray::const_iterator end = properties.end();
    for (PropertyNameArray::const_iterator iter = properties.begin(); iter != end; ++iter) {
        Identifier propertyName = *iter;
        if (exec->propertyNames().isPrivateName(propertyName))
            continue;
        // a. Let desc be the result of calling the [[GetOwnProperty]] internal method of O with P.
        PropertyDescriptor desc;
        if (!object->getOwnPropertyDescriptor(exec, propertyName, desc))
            continue;
        // b. If IsDataDescriptor(desc) is true, then
        // i. If desc.[[Writable]] is true, set desc.[[Writable]] to false.
        if (desc.isDataDescriptor())
            desc.setWritable(false);
        // c. If desc.[[Configurable]] is true, set desc.[[Configurable]] to false.
        desc.setConfigurable(false);
        // d. Call the [[DefineOwnProperty]] internal method of O with P, desc, and true as arguments.
        object->methodTable(exec->vm())->defineOwnProperty(object, exec, propertyName, desc, true);
        if (exec->hadException())
            return object;
    }

    // 3. Set the [[Extensible]] internal property of O to false.
    object->methodTable(exec->vm())->preventExtensions(object, exec);
    if (exec->hadException())
        return nullptr;

    // 4. Return O.
    return object;
}

EncodedJSValue JSC_HOST_CALL objectConstructorFreeze(ExecState* exec)
{
    // 1. If Type(O) is not Object, return O.
    JSValue obj = exec->argument(0);
    if (!obj.isObject())
        return JSValue::encode(obj);
    JSObject* result = objectConstructorFreeze(exec, asObject(obj));
    if (exec->hadException())
        return JSValue::encode(JSValue());
    return JSValue::encode(result);
}

EncodedJSValue JSC_HOST_CALL objectConstructorPreventExtensions(ExecState* exec)
{
    JSValue argument = exec->argument(0);
    if (!argument.isObject())
        return JSValue::encode(argument);
    JSObject* object = asObject(argument);
    object->methodTable(exec->vm())->preventExtensions(object, exec);
    return JSValue::encode(object);
}

EncodedJSValue JSC_HOST_CALL objectConstructorIsSealed(ExecState* exec)
{
    // 1. If Type(O) is not Object, return true.
    JSValue obj = exec->argument(0);
    if (!obj.isObject())
        return JSValue::encode(jsBoolean(true));
    JSObject* object = asObject(obj);

    if (isJSFinalObject(object))
        return JSValue::encode(jsBoolean(object->isSealed(exec->vm())));

    // 2. For each named own property name P of O,
    PropertyNameArray properties(exec, PropertyNameMode::StringsAndSymbols);
    object->methodTable(exec->vm())->getOwnPropertyNames(object, exec, properties, EnumerationMode(DontEnumPropertiesMode::Include));
    if (UNLIKELY(exec->hadException()))
        return JSValue::encode(JSValue());
    PropertyNameArray::const_iterator end = properties.end();
    for (PropertyNameArray::const_iterator iter = properties.begin(); iter != end; ++iter) {
        Identifier propertyName = *iter;
        if (exec->propertyNames().isPrivateName(propertyName))
            continue;
        // a. Let desc be the result of calling the [[GetOwnProperty]] internal method of O with P.
        PropertyDescriptor desc;
        if (!object->getOwnPropertyDescriptor(exec, propertyName, desc))
            continue;
        // b. If desc.[[Configurable]] is true, then return false.
        if (desc.configurable())
            return JSValue::encode(jsBoolean(false));
    }

    // 3. If the [[Extensible]] internal property of O is false, then return true.
    // 4. Otherwise, return false.
    bool isExtensible = object->isExtensible(exec);
    if (exec->hadException())
        return JSValue::encode(JSValue());
    return JSValue::encode(jsBoolean(!isExtensible));
}

EncodedJSValue JSC_HOST_CALL objectConstructorIsFrozen(ExecState* exec)
{
    // 1. If Type(O) is not Object, return true.
    JSValue obj = exec->argument(0);
    if (!obj.isObject())
        return JSValue::encode(jsBoolean(true));
    JSObject* object = asObject(obj);

    if (isJSFinalObject(object))
        return JSValue::encode(jsBoolean(object->isFrozen(exec->vm())));

    // 2. For each named own property name P of O,
    PropertyNameArray properties(exec, PropertyNameMode::StringsAndSymbols);
    object->methodTable(exec->vm())->getOwnPropertyNames(object, exec, properties, EnumerationMode(DontEnumPropertiesMode::Include));
    if (UNLIKELY(exec->hadException()))
        return JSValue::encode(JSValue());
    PropertyNameArray::const_iterator end = properties.end();
    for (PropertyNameArray::const_iterator iter = properties.begin(); iter != end; ++iter) {
        Identifier propertyName = *iter;
        if (exec->propertyNames().isPrivateName(propertyName))
            continue;
        // a. Let desc be the result of calling the [[GetOwnProperty]] internal method of O with P.
        PropertyDescriptor desc;
        if (!object->getOwnPropertyDescriptor(exec, propertyName, desc))
            continue;
        // b. If IsDataDescriptor(desc) is true then
        // i. If desc.[[Writable]] is true, return false. c. If desc.[[Configurable]] is true, then return false.
        if ((desc.isDataDescriptor() && desc.writable()) || desc.configurable())
            return JSValue::encode(jsBoolean(false));
    }

    // 3. If the [[Extensible]] internal property of O is false, then return true.
    // 4. Otherwise, return false.
    bool isExtensible = object->isExtensible(exec);
    if (exec->hadException())
        return JSValue::encode(JSValue());
    return JSValue::encode(jsBoolean(!isExtensible));
}

EncodedJSValue JSC_HOST_CALL objectConstructorIsExtensible(ExecState* exec)
{
    JSValue obj = exec->argument(0);
    if (!obj.isObject())
        return JSValue::encode(jsBoolean(false));
    JSObject* object = asObject(obj);
    bool isExtensible = object->isExtensible(exec);
    if (exec->hadException())
        return JSValue::encode(JSValue());
    return JSValue::encode(jsBoolean(isExtensible));
}

EncodedJSValue JSC_HOST_CALL objectConstructorIs(ExecState* exec)
{
    return JSValue::encode(jsBoolean(sameValue(exec, exec->argument(0), exec->argument(1))));
}

// FIXME: Use the enumeration cache.
JSArray* ownPropertyKeys(ExecState* exec, JSObject* object, PropertyNameMode propertyNameMode, DontEnumPropertiesMode dontEnumPropertiesMode)
{
    PropertyNameArray properties(exec, propertyNameMode);
    object->methodTable(exec->vm())->getOwnPropertyNames(object, exec, properties, EnumerationMode(dontEnumPropertiesMode));
    if (exec->hadException())
        return nullptr;

    JSArray* keys = constructEmptyArray(exec, 0);

    switch (propertyNameMode) {
    case PropertyNameMode::Strings: {
        size_t numProperties = properties.size();
        for (size_t i = 0; i < numProperties; i++) {
            const auto& identifier = properties[i];
            ASSERT(!identifier.isSymbol());
            keys->push(exec, jsOwnedString(exec, identifier.string()));
        }
        break;
    }

    case PropertyNameMode::Symbols: {
        size_t numProperties = properties.size();
        for (size_t i = 0; i < numProperties; i++) {
            const auto& identifier = properties[i];
            ASSERT(identifier.isSymbol());
            if (!exec->propertyNames().isPrivateName(identifier))
                keys->push(exec, Symbol::create(exec->vm(), static_cast<SymbolImpl&>(*identifier.impl())));
        }
        break;
    }

    case PropertyNameMode::StringsAndSymbols: {
        Vector<Identifier, 16> propertySymbols;
        size_t numProperties = properties.size();
        for (size_t i = 0; i < numProperties; i++) {
            const auto& identifier = properties[i];
            if (identifier.isSymbol()) {
                if (!exec->propertyNames().isPrivateName(identifier))
                    propertySymbols.append(identifier);
            } else
                keys->push(exec, jsOwnedString(exec, identifier.string()));
        }

        // To ensure the order defined in the spec (9.1.12), we append symbols at the last elements of keys.
        for (const auto& identifier : propertySymbols)
            keys->push(exec, Symbol::create(exec->vm(), static_cast<SymbolImpl&>(*identifier.impl())));

        break;
    }
    }

    return keys;
}

} // namespace JSC
