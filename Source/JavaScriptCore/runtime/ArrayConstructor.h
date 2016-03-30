/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2007, 2008, 2011 Apple Inc. All rights reserved.
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

#ifndef ArrayConstructor_h
#define ArrayConstructor_h

#include "InternalFunction.h"
#include "ProxyObject.h"

namespace JSC {

class ArrayAllocationProfile;
class ArrayPrototype;
class JSArray;
class GetterSetter;

class ArrayConstructor : public InternalFunction {
public:
    typedef InternalFunction Base;
    static const unsigned StructureFlags = OverridesGetOwnPropertySlot | InternalFunction::StructureFlags;

    static ArrayConstructor* create(VM& vm, JSGlobalObject* globalObject, Structure* structure, ArrayPrototype* arrayPrototype, GetterSetter* speciesSymbol)
    {
        ArrayConstructor* constructor = new (NotNull, allocateCell<ArrayConstructor>(vm.heap)) ArrayConstructor(vm, structure);
        constructor->finishCreation(vm, globalObject, arrayPrototype, speciesSymbol);
        return constructor;
    }

    DECLARE_INFO;

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
    }

protected:
    void finishCreation(VM&, JSGlobalObject*, ArrayPrototype*, GetterSetter* speciesSymbol);

private:
    ArrayConstructor(VM&, Structure*);
    static bool getOwnPropertySlot(JSObject*, ExecState*, PropertyName, PropertySlot&);

    static ConstructType getConstructData(JSCell*, ConstructData&);
    static CallType getCallData(JSCell*, CallData&);
};

JSObject* constructArrayWithSizeQuirk(ExecState*, ArrayAllocationProfile*, JSGlobalObject*, JSValue length, JSValue prototype = JSValue());

EncodedJSValue JSC_HOST_CALL arrayConstructorPrivateFuncIsArrayConstructor(ExecState*);

// ES6 7.2.2
// https://tc39.github.io/ecma262/#sec-isarray
inline bool isArray(ExecState* exec, JSValue argumentValue)
{
    if (!argumentValue.isObject())
        return false;

    JSObject* argument = jsCast<JSObject*>(argumentValue);
    while (true) {
        if (argument->inherits(JSArray::info()))
            return true;

        if (argument->type() != ProxyObjectType)
            return false;

        ProxyObject* proxy = jsCast<ProxyObject*>(argument);
        if (proxy->isRevoked()) {
            throwTypeError(exec, ASCIILiteral("Array.isArray cannot be called on a Proxy that has been revoked"));
            return false;
        }
        argument = proxy->target();
    }

    ASSERT_NOT_REACHED();
}

inline bool isArrayConstructor(JSValue argumentValue)
{
    if (!argumentValue.isObject())
        return false;

    return jsCast<JSObject*>(argumentValue)->classInfo() == ArrayConstructor::info();
}

} // namespace JSC

#endif // ArrayConstructor_h
