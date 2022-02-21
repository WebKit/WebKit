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
#include "MathCommon.h"

namespace JSC {

class NumberPrototype;
class GetterSetter;

class NumberConstructor final : public InternalFunction {
public:
    typedef InternalFunction Base;
    static constexpr unsigned StructureFlags = Base::StructureFlags | ImplementsHasInstance | HasStaticPropertyTable;

    static NumberConstructor* create(VM& vm, Structure* structure, NumberPrototype* numberPrototype, GetterSetter*)
    {
        NumberConstructor* constructor = new (NotNull, allocateCell<NumberConstructor>(vm)) NumberConstructor(vm, structure);
        constructor->finishCreation(vm, numberPrototype);
        return constructor;
    }

    DECLARE_INFO;

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue proto) 
    { 
        return Structure::create(vm, globalObject, proto, TypeInfo(InternalFunctionType, StructureFlags), info()); 
    }

    static bool isIntegerImpl(JSValue value)
    {
        return value.isInt32() || (value.isDouble() && isInteger(value.asDouble()));
    }

private:
    NumberConstructor(VM&, Structure*);
    void finishCreation(VM&, NumberPrototype*);
};
STATIC_ASSERT_ISO_SUBSPACE_SHARABLE(NumberConstructor, InternalFunction);

} // namespace JSC
