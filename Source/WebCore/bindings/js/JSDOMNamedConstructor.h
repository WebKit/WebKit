/*
 *  Copyright (C) 2015, 2016 Canon Inc. All rights reserved.
 *  Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include "JSDOMConstructorWithDocument.h"

namespace WebCore {

// FIMXE: Why can't named constructors be used with workers?
template<typename JSClass> class JSDOMNamedConstructor : public JSDOMConstructorWithDocument {
public:
    using Base = JSDOMConstructorWithDocument;

    static JSDOMNamedConstructor* create(JSC::VM&, JSC::Structure*, JSDOMGlobalObject&);
    static JSC::Structure* createStructure(JSC::VM&, JSC::JSGlobalObject&, JSC::JSValue prototype);

    DECLARE_INFO;

    // Must be defined for each specialization class.
    static JSC::JSValue prototypeForStructure(JSC::VM&, const JSDOMGlobalObject&);

private:
    JSDOMNamedConstructor(JSC::Structure* structure, JSDOMGlobalObject& globalObject)
        : Base(structure, globalObject)
    { 
    }

    void finishCreation(JSC::VM&, JSDOMGlobalObject&);
    static JSC::ConstructType getConstructData(JSC::JSCell*, JSC::ConstructData&);

    // Usually defined for each specialization class.
    void initializeProperties(JSC::VM&, JSDOMGlobalObject&) { }
    // Must be defined for each specialization class.
    static JSC::EncodedJSValue JSC_HOST_CALL construct(JSC::ExecState*);
};

template<typename JSClass> inline JSDOMNamedConstructor<JSClass>* JSDOMNamedConstructor<JSClass>::create(JSC::VM& vm, JSC::Structure* structure, JSDOMGlobalObject& globalObject)
{
    JSDOMNamedConstructor* constructor = new (NotNull, JSC::allocateCell<JSDOMNamedConstructor>(vm.heap)) JSDOMNamedConstructor(structure, globalObject);
    constructor->finishCreation(vm, globalObject);
    return constructor;
}

template<typename JSClass> inline JSC::Structure* JSDOMNamedConstructor<JSClass>::createStructure(JSC::VM& vm, JSC::JSGlobalObject& globalObject, JSC::JSValue prototype)
{
    return JSC::Structure::create(vm, &globalObject, prototype, JSC::TypeInfo(JSC::ObjectType, StructureFlags), info());
}

template<typename JSClass> inline void JSDOMNamedConstructor<JSClass>::finishCreation(JSC::VM& vm, JSDOMGlobalObject& globalObject)
{
    Base::finishCreation(globalObject);
    ASSERT(inherits(vm, info()));
    initializeProperties(vm, globalObject);
}

template<typename JSClass> inline JSC::ConstructType JSDOMNamedConstructor<JSClass>::getConstructData(JSC::JSCell*, JSC::ConstructData& constructData)
{
    constructData.native.function = construct;
    return JSC::ConstructType::Host;
}

} // namespace WebCore
