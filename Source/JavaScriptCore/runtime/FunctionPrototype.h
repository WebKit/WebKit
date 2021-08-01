/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
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

class FunctionPrototype final : public InternalFunction {
public:
    typedef InternalFunction Base;

    static FunctionPrototype* create(VM& vm, Structure* structure)
    {
        FunctionPrototype* prototype = new (NotNull, allocateCell<FunctionPrototype>(vm.heap)) FunctionPrototype(vm, structure);
        prototype->finishCreation(vm, String());
        return prototype;
    }

    void addFunctionProperties(VM&, JSGlobalObject*, JSFunction** callFunction, JSFunction** applyFunction, JSFunction** hasInstanceSymbolFunction);

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue proto)
    {
        return Structure::create(vm, globalObject, proto, TypeInfo(InternalFunctionType, StructureFlags), info());
    }

    DECLARE_INFO;

private:
    FunctionPrototype(VM&, Structure*);
    void finishCreation(VM&, const String& name);
};
STATIC_ASSERT_ISO_SUBSPACE_SHARABLE(FunctionPrototype, InternalFunction);

} // namespace JSC
