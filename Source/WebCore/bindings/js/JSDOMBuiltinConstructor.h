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

#include "JSDOMBuiltinConstructorBase.h"
#include "JSDOMExceptionHandling.h"
#include "JSDOMWrapperCache.h"

namespace WebCore {

template<typename JSClass> class JSDOMBuiltinConstructor final : public JSDOMBuiltinConstructorBase {
public:
    using Base = JSDOMBuiltinConstructorBase;

    static JSDOMBuiltinConstructor* create(JSC::VM&, JSC::Structure*, JSDOMGlobalObject&);
    static JSC::Structure* createStructure(JSC::VM&, JSC::JSGlobalObject&, JSC::JSValue prototype);

    DECLARE_INFO;

    // Usually defined for each specialization class.
    static JSC::JSValue prototypeForStructure(JSC::VM&, const JSDOMGlobalObject&);

private:
    JSDOMBuiltinConstructor(JSC::Structure* structure, JSDOMGlobalObject& globalObject)
        : Base(structure, globalObject)
    {
    }

    void finishCreation(JSC::VM&, JSDOMGlobalObject&);
    static JSC::CallData getConstructData(JSC::JSCell*);
    static JSC::EncodedJSValue JSC_HOST_CALL_ATTRIBUTES construct(JSC::JSGlobalObject*, JSC::CallFrame*);

    JSC::EncodedJSValue callConstructor(JSC::JSGlobalObject&, JSC::CallFrame&, JSC::JSObject&);
    JSC::EncodedJSValue callConstructor(JSC::JSGlobalObject&, JSC::CallFrame&, JSC::JSObject*);

    // Usually defined for each specialization class.
    void initializeProperties(JSC::VM&, JSDOMGlobalObject&) { }
    // Must be defined for each specialization class.
    JSC::FunctionExecutable* initializeExecutable(JSC::VM&);
};

template<typename JSClass> inline JSDOMBuiltinConstructor<JSClass>* JSDOMBuiltinConstructor<JSClass>::create(JSC::VM& vm, JSC::Structure* structure, JSDOMGlobalObject& globalObject)
{
    JSDOMBuiltinConstructor* constructor = new (NotNull, JSC::allocateCell<JSDOMBuiltinConstructor>(vm.heap)) JSDOMBuiltinConstructor(structure, globalObject);
    constructor->finishCreation(vm, globalObject);
    return constructor;
}

template<typename JSClass> inline JSC::Structure* JSDOMBuiltinConstructor<JSClass>::createStructure(JSC::VM& vm, JSC::JSGlobalObject& globalObject, JSC::JSValue prototype)
{
    return JSC::Structure::create(vm, &globalObject, prototype, JSC::TypeInfo(JSC::ObjectType, StructureFlags), info());
}

template<typename JSClass> inline void JSDOMBuiltinConstructor<JSClass>::finishCreation(JSC::VM& vm, JSDOMGlobalObject& globalObject)
{
    Base::finishCreation(vm);
    ASSERT(inherits(vm, info()));
    setInitializeFunction(vm, *JSC::JSFunction::create(vm, initializeExecutable(vm), &globalObject));
    initializeProperties(vm, globalObject);
}

template<typename JSClass> inline JSC::EncodedJSValue JSDOMBuiltinConstructor<JSClass>::callConstructor(JSC::JSGlobalObject& lexicalGlobalObject, JSC::CallFrame& callFrame, JSC::JSObject& object)
{
    Base::callFunctionWithCurrentArguments(lexicalGlobalObject, callFrame, object, *initializeFunction());
    return JSC::JSValue::encode(&object);
}

template<typename JSClass> inline JSC::EncodedJSValue JSDOMBuiltinConstructor<JSClass>::callConstructor(JSC::JSGlobalObject& lexicalGlobalObject, JSC::CallFrame& callFrame, JSC::JSObject* object)
{
    JSC::VM& vm = JSC::getVM(&lexicalGlobalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);
    if (!object)
        return throwConstructorScriptExecutionContextUnavailableError(lexicalGlobalObject, scope, info()->className);
    return callConstructor(lexicalGlobalObject, callFrame, *object);
}

template<typename JSClass>
typename std::enable_if<JSDOMObjectInspector<JSClass>::isSimpleWrapper, JSC::JSObject&>::type createJSObject(JSDOMBuiltinConstructor<JSClass>& constructor)
{
    return *createWrapper<typename JSClass::DOMWrapped>(constructor.globalObject(), JSClass::DOMWrapped::create());
}

template<typename JSClass>
typename std::enable_if<JSDOMObjectInspector<JSClass>::isBuiltin, JSC::JSObject&>::type createJSObject(JSDOMBuiltinConstructor<JSClass>& constructor)
{
    auto& globalObject = *constructor.globalObject();
    return *JSClass::create(getDOMStructure<JSClass>(globalObject.vm(), globalObject), &globalObject);
}

template<typename JSClass>
typename std::enable_if<JSDOMObjectInspector<JSClass>::isComplexWrapper, JSC::JSObject*>::type createJSObject(JSDOMBuiltinConstructor<JSClass>& constructor)
{
    auto* context = constructor.scriptExecutionContext();
    return context ? createWrapper<typename JSClass::DOMWrapped>(constructor.globalObject(), JSClass::DOMWrapped::create(*context)) : nullptr;
}

template<typename JSClass> inline JSC::EncodedJSValue JSC_HOST_CALL_ATTRIBUTES JSDOMBuiltinConstructor<JSClass>::construct(JSC::JSGlobalObject* lexicalGlobalObject, JSC::CallFrame* callFrame)
{
    ASSERT(callFrame);
    auto* castedThis = JSC::jsCast<JSDOMBuiltinConstructor*>(callFrame->jsCallee());
    return castedThis->callConstructor(*lexicalGlobalObject, *callFrame, createJSObject(*castedThis));
}

template<typename JSClass> inline JSC::CallData JSDOMBuiltinConstructor<JSClass>::getConstructData(JSC::JSCell*)
{
    JSC::CallData constructData;
    constructData.type = JSC::CallData::Type::Native;
    constructData.native.function = construct;
    return constructData;
}

} // namespace WebCore
