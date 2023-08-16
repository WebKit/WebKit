/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003-2022 Apple Inc. All Rights Reserved.
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
#include "RegExp.h"

namespace JSC {

class RegExpPrototype final : public JSNonFinalObject {
public:
    using Base = JSNonFinalObject;
    static constexpr unsigned StructureFlags = Base::StructureFlags;

    template<typename CellType, SubspaceAccess>
    static GCClient::IsoSubspace* subspaceFor(VM& vm)
    {
        STATIC_ASSERT_ISO_SUBSPACE_SHARABLE(RegExpPrototype, Base);
        return &vm.plainObjectSpace();
    }

    static RegExpPrototype* create(VM& vm, JSGlobalObject* globalObject, Structure* structure)
    {
        RegExpPrototype* prototype = new (NotNull, allocateCell<RegExpPrototype>(vm)) RegExpPrototype(vm, structure);
        prototype->finishCreation(vm, globalObject);
        return prototype;
    }

    DECLARE_INFO;

    inline static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

private:
    RegExpPrototype(VM&, Structure*);
    void finishCreation(VM&, JSGlobalObject*);
};

JSC_DECLARE_HOST_FUNCTION(regExpProtoFuncMatchFast);
JSC_DECLARE_HOST_FUNCTION(regExpProtoFuncSearchFast);
JSC_DECLARE_HOST_FUNCTION(regExpProtoFuncSplitFast);
JSC_DECLARE_HOST_FUNCTION(regExpProtoFuncTestFast);

} // namespace JSC
