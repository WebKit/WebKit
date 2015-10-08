/*
 *  Copyright (C) 2015 Canon Inc. All rights reserved.
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

#ifndef JSDOMConstructor_h
#define JSDOMConstructor_h

#include "JSDOMBinding.h"

namespace WebCore {

template <typename JSClass> class JSBuiltinConstructor : public DOMConstructorJSBuiltinObject {
public:
    typedef DOMConstructorJSBuiltinObject Base;

    static JSBuiltinConstructor* create(JSC::VM& vm, JSC::Structure* structure, JSDOMGlobalObject& globalObject)
    {
        JSBuiltinConstructor* constructor = new (NotNull, JSC::allocateCell<JSBuiltinConstructor>(vm.heap)) JSBuiltinConstructor(structure, globalObject);
        constructor->finishCreation(vm, globalObject);
        return constructor;
    }

    static JSC::Structure* createStructure(JSC::VM& vm, JSC::JSGlobalObject& globalObject, JSC::JSValue prototype)
    {
        return JSC::Structure::create(vm, &globalObject, prototype, JSC::TypeInfo(JSC::ObjectType, StructureFlags), info());
    }

    DECLARE_INFO;

private:
    JSBuiltinConstructor(JSC::Structure* structure, JSDOMGlobalObject& globalObject) : Base(structure, globalObject) { }

    void finishCreation(JSC::VM&, JSDOMGlobalObject&);
    void initializeProperties(JSC::VM&, JSDOMGlobalObject&) { }
    static JSC::ConstructType getConstructData(JSC::JSCell*, JSC::ConstructData&);
    static JSC::EncodedJSValue JSC_HOST_CALL construct(JSC::ExecState*);

    // Must be defined for each specialization class.
    JSC::JSFunction* createInitializeFunction(JSC::VM&, JSC::JSGlobalObject&);
    JSC::JSObject* createJSObject();
};

template<typename JSClass> void JSBuiltinConstructor<JSClass>::finishCreation(JSC::VM& vm, JSDOMGlobalObject& globalObject)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
    setInitializeFunction(vm, *createInitializeFunction(vm, globalObject));
    initializeProperties(vm, globalObject);
}

template<typename JSClass> JSC::EncodedJSValue JSC_HOST_CALL JSBuiltinConstructor<JSClass>::construct(JSC::ExecState* state)
{
    auto* castedThis = JSC::jsCast<JSBuiltinConstructor*>(state->callee());
    JSObject* object = castedThis->createJSObject();
    callFunctionWithCurrentArguments(*state, *object, *castedThis->initializeFunction());
    return JSC::JSValue::encode(object);
}

template<typename JSClass> JSC::ConstructType JSBuiltinConstructor<JSClass>::getConstructData(JSC::JSCell*, JSC::ConstructData& constructData)
{
    constructData.native.function = construct;
    return JSC::ConstructTypeHost;
}

} // namespace WebCore

#endif // JSDOMConstructor_h
