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

#include "DOMConstructorWithDocument.h"
#include "JSDOMBinding.h"

namespace WebCore {

template<typename JSClass> class JSDOMConstructorNotConstructable : public DOMConstructorObject {
public:
    typedef DOMConstructorObject Base;

    static JSDOMConstructorNotConstructable* create(JSC::VM&, JSC::Structure*, JSDOMGlobalObject&);
    static JSC::Structure* createStructure(JSC::VM&, JSC::JSGlobalObject&, JSC::JSValue prototype);

    DECLARE_INFO;

    // Must be defined for each specialization class.
    static JSC::JSValue prototypeForStructure(JSC::VM&, const JSDOMGlobalObject&);

private:
    JSDOMConstructorNotConstructable(JSC::Structure* structure, JSDOMGlobalObject& globalObject) : Base(structure, globalObject) { }
    void finishCreation(JSC::VM&, JSDOMGlobalObject&);

    // Usually defined for each specialization class.
    void initializeProperties(JSC::VM&, JSDOMGlobalObject&) { }

    static JSC::EncodedJSValue JSC_HOST_CALL callThrowTypeError(JSC::ExecState* exec)
    {
        JSC::throwTypeError(exec, ASCIILiteral("Illegal constructor"));
        return JSC::JSValue::encode(JSC::jsNull());
    }

    static JSC::CallType getCallData(JSC::JSCell*, JSC::CallData& callData)
    {
        callData.native.function = callThrowTypeError;
        return JSC::CallTypeHost;
    }
};

template<typename JSClass> class JSDOMConstructor : public DOMConstructorObject {
public:
    typedef DOMConstructorObject Base;

    static JSDOMConstructor* create(JSC::VM&, JSC::Structure*, JSDOMGlobalObject&);
    static JSC::Structure* createStructure(JSC::VM&, JSC::JSGlobalObject&, JSC::JSValue prototype);

    DECLARE_INFO;

    // Must be defined for each specialization class.
    static JSC::JSValue prototypeForStructure(JSC::VM&, const JSDOMGlobalObject&);

private:
    JSDOMConstructor(JSC::Structure* structure, JSDOMGlobalObject& globalObject) : Base(structure, globalObject) { }
    void finishCreation(JSC::VM&, JSDOMGlobalObject&);
    static JSC::ConstructType getConstructData(JSC::JSCell*, JSC::ConstructData&);

    // Usually defined for each specialization class.
    void initializeProperties(JSC::VM&, JSDOMGlobalObject&) { }
    // Must be defined for each specialization class.
    static JSC::EncodedJSValue JSC_HOST_CALL construct(JSC::ExecState*);
};

template<typename JSClass> class JSDOMNamedConstructor : public DOMConstructorWithDocument {
public:
    typedef DOMConstructorWithDocument Base;

    static JSDOMNamedConstructor* create(JSC::VM&, JSC::Structure*, JSDOMGlobalObject&);
    static JSC::Structure* createStructure(JSC::VM&, JSC::JSGlobalObject&, JSC::JSValue prototype);

    DECLARE_INFO;

    // Must be defined for each specialization class.
    static JSC::JSValue prototypeForStructure(JSC::VM&, const JSDOMGlobalObject&);

private:
    JSDOMNamedConstructor(JSC::Structure* structure, JSDOMGlobalObject& globalObject) : Base(structure, globalObject) { }
    void finishCreation(JSC::VM&, JSDOMGlobalObject&);
    static JSC::ConstructType getConstructData(JSC::JSCell*, JSC::ConstructData&);

    // Usually defined for each specialization class.
    void initializeProperties(JSC::VM&, JSDOMGlobalObject&) { }
    // Must be defined for each specialization class.
    static JSC::EncodedJSValue JSC_HOST_CALL construct(JSC::ExecState*);
};

template<typename JSClass> class JSBuiltinConstructor : public DOMConstructorJSBuiltinObject {
public:
    typedef DOMConstructorJSBuiltinObject Base;

    static JSBuiltinConstructor* create(JSC::VM&, JSC::Structure*, JSDOMGlobalObject&);
    static JSC::Structure* createStructure(JSC::VM&, JSC::JSGlobalObject&, JSC::JSValue prototype);

    DECLARE_INFO;

    // Usually defined for each specialization class.
    static JSC::JSValue prototypeForStructure(JSC::VM&, const JSDOMGlobalObject&);

private:
    JSBuiltinConstructor(JSC::Structure* structure, JSDOMGlobalObject& globalObject) : Base(structure, globalObject) { }
    void finishCreation(JSC::VM&, JSDOMGlobalObject&);
    static JSC::ConstructType getConstructData(JSC::JSCell*, JSC::ConstructData&);
    static JSC::EncodedJSValue JSC_HOST_CALL construct(JSC::ExecState*);
    JSC::JSObject* createJSObject();

    // Usually defined for each specialization class.
    void initializeProperties(JSC::VM&, JSDOMGlobalObject&) { }
    // Must be defined for each specialization class.
    JSC::FunctionExecutable* initializeExecutable(JSC::VM&);
};

template<typename JSClass> inline JSDOMConstructorNotConstructable<JSClass>* JSDOMConstructorNotConstructable<JSClass>::create(JSC::VM& vm, JSC::Structure* structure, JSDOMGlobalObject& globalObject)
{
    JSDOMConstructorNotConstructable* constructor = new (NotNull, JSC::allocateCell<JSDOMConstructorNotConstructable>(vm.heap)) JSDOMConstructorNotConstructable(structure, globalObject);
    constructor->finishCreation(vm, globalObject);
    return constructor;
}

template<typename JSClass> inline JSC::Structure* JSDOMConstructorNotConstructable<JSClass>::createStructure(JSC::VM& vm, JSC::JSGlobalObject& globalObject, JSC::JSValue prototype)
{
    return JSC::Structure::create(vm, &globalObject, prototype, JSC::TypeInfo(JSC::ObjectType, StructureFlags), info());
}

template<typename JSClass> inline void JSDOMConstructorNotConstructable<JSClass>::finishCreation(JSC::VM& vm, JSDOMGlobalObject& globalObject)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
    initializeProperties(vm, globalObject);
}

template<typename JSClass> inline JSDOMConstructor<JSClass>* JSDOMConstructor<JSClass>::create(JSC::VM& vm, JSC::Structure* structure, JSDOMGlobalObject& globalObject)
{
    JSDOMConstructor* constructor = new (NotNull, JSC::allocateCell<JSDOMConstructor>(vm.heap)) JSDOMConstructor(structure, globalObject);
    constructor->finishCreation(vm, globalObject);
    return constructor;
}

template<typename JSClass> inline JSC::Structure* JSDOMConstructor<JSClass>::createStructure(JSC::VM& vm, JSC::JSGlobalObject& globalObject, JSC::JSValue prototype)
{
    return JSC::Structure::create(vm, &globalObject, prototype, JSC::TypeInfo(JSC::ObjectType, StructureFlags), info());
}

template<typename JSClass> inline void JSDOMConstructor<JSClass>::finishCreation(JSC::VM& vm, JSDOMGlobalObject& globalObject)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
    initializeProperties(vm, globalObject);
}

template<typename JSClass> inline JSC::ConstructType JSDOMConstructor<JSClass>::getConstructData(JSC::JSCell*, JSC::ConstructData& constructData)
{
    constructData.native.function = construct;
    return JSC::ConstructTypeHost;
}

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
    ASSERT(inherits(info()));
    initializeProperties(vm, globalObject);
}

template<typename JSClass> inline JSC::ConstructType JSDOMNamedConstructor<JSClass>::getConstructData(JSC::JSCell*, JSC::ConstructData& constructData)
{
    constructData.native.function = construct;
    return JSC::ConstructTypeHost;
}

template<typename JSClass> inline JSBuiltinConstructor<JSClass>* JSBuiltinConstructor<JSClass>::create(JSC::VM& vm, JSC::Structure* structure, JSDOMGlobalObject& globalObject)
{
    JSBuiltinConstructor* constructor = new (NotNull, JSC::allocateCell<JSBuiltinConstructor>(vm.heap)) JSBuiltinConstructor(structure, globalObject);
    constructor->finishCreation(vm, globalObject);
    return constructor;
}

template<typename JSClass> inline JSC::Structure* JSBuiltinConstructor<JSClass>::createStructure(JSC::VM& vm, JSC::JSGlobalObject& globalObject, JSC::JSValue prototype)
{
    return JSC::Structure::create(vm, &globalObject, prototype, JSC::TypeInfo(JSC::ObjectType, StructureFlags), info());
}

template<typename JSClass> inline void JSBuiltinConstructor<JSClass>::finishCreation(JSC::VM& vm, JSDOMGlobalObject& globalObject)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
    setInitializeFunction(vm, *JSC::JSFunction::createBuiltinFunction(vm, initializeExecutable(vm), &globalObject));
    initializeProperties(vm, globalObject);
}

template<typename JSClass> inline JSC::EncodedJSValue JSC_HOST_CALL JSBuiltinConstructor<JSClass>::construct(JSC::ExecState* state)
{
    auto* castedThis = JSC::jsCast<JSBuiltinConstructor*>(state->callee());
    auto* object = castedThis->createJSObject();
    callFunctionWithCurrentArguments(*state, *object, *castedThis->initializeFunction());
    return JSC::JSValue::encode(object);
}

template<typename JSClass> inline JSC::JSObject* JSBuiltinConstructor<JSClass>::createJSObject()
{
    return JSClass::create(getDOMStructure<JSClass>(globalObject()->vm(), *globalObject()), globalObject());
}

template<typename JSClass> inline JSC::ConstructType JSBuiltinConstructor<JSClass>::getConstructData(JSC::JSCell*, JSC::ConstructData& constructData)
{
    constructData.native.function = construct;
    return JSC::ConstructTypeHost;
}

} // namespace WebCore

#endif // JSDOMConstructor_h
