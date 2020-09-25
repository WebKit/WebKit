/*
 *  Copyright (C) 2015, 2016 Canon Inc. All rights reserved.
 *  Copyright (C) 2016-2019 Apple Inc. All rights reserved.
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
 */

#pragma once

#include "JSDOMWrapper.h"

namespace WebCore {

JSC_DECLARE_HOST_FUNCTION(callThrowTypeErrorForJSDOMConstructorNotConstructable);

// Base class for all constructor objects in the JSC bindings.
class JSDOMConstructorBase : public JSDOMObject {
public:
    using Base = JSDOMObject;

    static constexpr unsigned StructureFlags = Base::StructureFlags | JSC::ImplementsHasInstance | JSC::ImplementsDefaultHasInstance | JSC::OverridesGetCallData;
    static constexpr bool needsDestruction = false;
    static JSC::Structure* createStructure(JSC::VM&, JSC::JSGlobalObject*, JSC::JSValue);

    template<typename CellType, JSC::SubspaceAccess>
    static JSC::IsoSubspace* subspaceFor(JSC::VM& vm)
    {
        static_assert(sizeof(CellType) == sizeof(JSDOMConstructorBase));
        STATIC_ASSERT_ISO_SUBSPACE_SHARABLE(CellType, JSDOMConstructorBase);
        static_assert(CellType::destroy == JSC::JSCell::destroy, "JSDOMConstructor<JSClass> is not destructible actually");
        return subspaceForImpl(vm);
    }

    static JSC::IsoSubspace* subspaceForImpl(JSC::VM&);

protected:
    JSDOMConstructorBase(JSC::Structure* structure, JSDOMGlobalObject& globalObject)
        : JSDOMObject(structure, globalObject)
    {
    }

    static JSC::CallData getCallData(JSC::JSCell*);
};

inline JSC::Structure* JSDOMConstructorBase::createStructure(JSC::VM& vm, JSC::JSGlobalObject* globalObject, JSC::JSValue prototype)
{
    return JSC::Structure::create(vm, globalObject, prototype, JSC::TypeInfo(JSC::ObjectType, StructureFlags), info());
}

} // namespace WebCore
