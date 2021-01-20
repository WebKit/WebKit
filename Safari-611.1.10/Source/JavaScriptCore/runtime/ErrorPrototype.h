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

#pragma once

#include "JSObject.h"

namespace JSC {

class ObjectPrototype;

// Superclass for ErrorPrototype, NativeErrorPrototype, and AggregateErrorPrototype.
class ErrorPrototypeBase : public JSNonFinalObject {
public:
    using Base = JSNonFinalObject;

protected:
    ErrorPrototypeBase(VM&, Structure*);
    void finishCreation(VM&, const String&);
};

class ErrorPrototype final : public ErrorPrototypeBase {
public:
    using Base = ErrorPrototypeBase;
    static constexpr unsigned StructureFlags = Base::StructureFlags | HasStaticPropertyTable;

    template<typename CellType, SubspaceAccess>
    static IsoSubspace* subspaceFor(VM& vm)
    {
        STATIC_ASSERT_ISO_SUBSPACE_SHARABLE(ErrorPrototypeBase, Base);
        return &vm.plainObjectSpace;
    }

    DECLARE_INFO;

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
    }

    static ErrorPrototype* create(VM& vm, JSGlobalObject*, Structure* structure)
    {
        ErrorPrototype* prototype = new (NotNull, allocateCell<ErrorPrototype>(vm.heap)) ErrorPrototype(vm, structure);
        prototype->finishCreation(vm, "Error"_s);
        return prototype;
    }

private:
    ErrorPrototype(VM&, Structure*);
};

} // namespace JSC
