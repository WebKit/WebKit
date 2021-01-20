/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2008, 2016 Apple Inc. All rights reserved.
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

#include "ErrorPrototype.h"

namespace JSC {

class NativeErrorPrototype final : public ErrorPrototypeBase {
private:
    NativeErrorPrototype(VM&, Structure*);

public:
    using Base = ErrorPrototypeBase;
    template<typename CellType, SubspaceAccess>
    static IsoSubspace* subspaceFor(VM& vm)
    {
        STATIC_ASSERT_ISO_SUBSPACE_SHARABLE(NativeErrorPrototype, Base);
        return &vm.plainObjectSpace;
    }

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
    }

    static NativeErrorPrototype* create(VM& vm, Structure* structure, const String& name)
    {
        NativeErrorPrototype* prototype = new (NotNull, allocateCell<NativeErrorPrototype>(vm.heap)) NativeErrorPrototype(vm, structure);
        prototype->finishCreation(vm, name);
        return prototype;
    }
};

} // namespace JSC
