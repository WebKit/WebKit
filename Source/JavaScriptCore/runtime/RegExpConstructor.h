/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003-2019 Apple Inc. All Rights Reserved.
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

#pragma once

#include "InternalFunction.h"
#include "RegExp.h"
#include "RegExpCachedResult.h"
#include "RegExpObject.h"

namespace JSC {

class RegExpPrototype;
class GetterSetter;

class RegExpConstructor final : public InternalFunction {
public:
    typedef InternalFunction Base;
    static constexpr unsigned StructureFlags = Base::StructureFlags | HasStaticPropertyTable;

    static RegExpConstructor* create(VM& vm, Structure* structure, RegExpPrototype* regExpPrototype, GetterSetter* species)
    {
        RegExpConstructor* constructor = new (NotNull, allocateCell<RegExpConstructor>(vm.heap)) RegExpConstructor(vm, structure);
        constructor->finishCreation(vm, regExpPrototype, species);
        return constructor;
    }

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(InternalFunctionType, StructureFlags), info());
    }

    DECLARE_INFO;

protected:
    void finishCreation(VM&, RegExpPrototype*, GetterSetter* species);

private:
    RegExpConstructor(VM&, Structure*);
};
STATIC_ASSERT_ISO_SUBSPACE_SHARABLE(RegExpConstructor, InternalFunction);

JSObject* constructRegExp(JSGlobalObject*, const ArgList&, JSObject* callee = nullptr, JSValue newTarget = jsUndefined());

ALWAYS_INLINE bool isRegExp(VM& vm, JSGlobalObject* globalObject, JSValue value)
{
    auto scope = DECLARE_THROW_SCOPE(vm);
    if (!value.isObject())
        return false;

    JSObject* object = asObject(value);
    JSValue matchValue = object->get(globalObject, vm.propertyNames->matchSymbol);
    RETURN_IF_EXCEPTION(scope, false);
    if (!matchValue.isUndefined())
        return matchValue.toBoolean(globalObject);

    return object->inherits<RegExpObject>(vm);
}

EncodedJSValue JSC_HOST_CALL esSpecRegExpCreate(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL esSpecIsRegExp(JSGlobalObject*, CallFrame*);

} // namespace JSC
