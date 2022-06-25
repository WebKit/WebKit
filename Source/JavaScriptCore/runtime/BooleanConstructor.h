/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2008-2021 Apple Inc. All rights reserved.
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

namespace JSC {

class BooleanPrototype;
class GetterSetter;

class BooleanConstructor final : public InternalFunction {
public:
    typedef InternalFunction Base;

    static BooleanConstructor* create(VM& vm, Structure* structure, BooleanPrototype* booleanPrototype, GetterSetter*)
    {
        BooleanConstructor* constructor = new (NotNull, allocateCell<BooleanConstructor>(vm)) BooleanConstructor(vm, structure);
        constructor->finishCreation(vm, booleanPrototype);
        return constructor;
    }

    DECLARE_INFO;

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype) 
    { 
        return Structure::create(vm, globalObject, prototype, TypeInfo(InternalFunctionType, StructureFlags), info()); 
    }

private:
    BooleanConstructor(VM&, Structure*);
    void finishCreation(VM&, BooleanPrototype*);
};
STATIC_ASSERT_ISO_SUBSPACE_SHARABLE(BooleanConstructor, InternalFunction);

JSObject* constructBooleanFromImmediateBoolean(JSGlobalObject*, JSValue);

} // namespace JSC
