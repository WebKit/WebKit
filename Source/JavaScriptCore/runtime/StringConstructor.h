/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
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

class StringPrototype;
class GetterSetter;

class StringConstructor final : public InternalFunction {
public:
    typedef InternalFunction Base;
    static const unsigned StructureFlags = Base::StructureFlags | HasStaticPropertyTable;

    static StringConstructor* create(VM& vm, Structure* structure, StringPrototype* stringPrototype, GetterSetter*)
    {
        StringConstructor* constructor = new (NotNull, allocateCell<StringConstructor>(vm.heap)) StringConstructor(vm, structure);
        constructor->finishCreation(vm, stringPrototype);
        return constructor;
    }

    DECLARE_INFO;

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(InternalFunctionType, StructureFlags), info());
    }

private:
    StringConstructor(VM&, Structure*);
    void finishCreation(VM&, StringPrototype*);
};

JSString* JSC_HOST_CALL stringFromCharCode(ExecState*, int32_t);
JSString* stringConstructor(ExecState*, JSValue);

} // namespace JSC
