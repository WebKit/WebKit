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

#ifndef ObjectConstructor_h
#define ObjectConstructor_h

#include "InternalFunction.h"
#include "JSGlobalObject.h"
#include "ObjectPrototype.h"

namespace JSC {

EncodedJSValue JSC_HOST_CALL objectConstructorGetOwnPropertyDescriptor(ExecState*);
EncodedJSValue JSC_HOST_CALL objectConstructorGetOwnPropertyDescriptors(ExecState*);
EncodedJSValue JSC_HOST_CALL objectConstructorGetOwnPropertySymbols(ExecState*);
EncodedJSValue JSC_HOST_CALL objectConstructorKeys(ExecState*);
EncodedJSValue JSC_HOST_CALL ownEnumerablePropertyKeys(ExecState*);

class ObjectPrototype;

class ObjectConstructor : public InternalFunction {
public:
    typedef InternalFunction Base;
    static const unsigned StructureFlags = Base::StructureFlags | OverridesGetOwnPropertySlot;

    static ObjectConstructor* create(VM& vm, JSGlobalObject* globalObject, Structure* structure, ObjectPrototype* objectPrototype)
    {
        ObjectConstructor* constructor = new (NotNull, allocateCell<ObjectConstructor>(vm.heap)) ObjectConstructor(vm, structure);
        constructor->finishCreation(vm, globalObject, objectPrototype);
        return constructor;
    }

    static bool getOwnPropertySlot(JSObject*, ExecState*, PropertyName, PropertySlot&);

    DECLARE_INFO;

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
    }

    JSFunction* addDefineProperty(ExecState*, JSGlobalObject*);

protected:
    void finishCreation(VM&, JSGlobalObject*, ObjectPrototype*);

private:
    ObjectConstructor(VM&, Structure*);
    static ConstructType getConstructData(JSCell*, ConstructData&);
    static CallType getCallData(JSCell*, CallData&);
};

inline JSObject* constructEmptyObject(ExecState* exec, Structure* structure)
{
    return JSFinalObject::create(exec, structure);
}

inline JSObject* constructEmptyObject(ExecState* exec, JSObject* prototype, unsigned inlineCapacity)
{
    JSGlobalObject* globalObject = exec->lexicalGlobalObject();
    PrototypeMap& prototypeMap = globalObject->vm().prototypeMap;
    Structure* structure = prototypeMap.emptyObjectStructureForPrototype(
        prototype, inlineCapacity);
    return constructEmptyObject(exec, structure);
}

inline JSObject* constructEmptyObject(ExecState* exec, JSObject* prototype)
{
    return constructEmptyObject(exec, prototype, JSFinalObject::defaultInlineCapacity());
}

inline JSObject* constructEmptyObject(ExecState* exec)
{
    return constructEmptyObject(exec, exec->lexicalGlobalObject()->objectPrototype());
}

JSObject* objectConstructorFreeze(ExecState*, JSObject*);
JSValue objectConstructorGetPrototypeOf(ExecState*, JSObject*);
JSValue objectConstructorGetOwnPropertyDescriptor(ExecState*, JSObject*, const Identifier&);
JSValue objectConstructorGetOwnPropertyDescriptors(ExecState*, JSObject*);
JSArray* ownPropertyKeys(ExecState*, JSObject*, PropertyNameMode, DontEnumPropertiesMode);
bool toPropertyDescriptor(ExecState*, JSValue, PropertyDescriptor&);

} // namespace JSC

#endif // ObjectConstructor_h
