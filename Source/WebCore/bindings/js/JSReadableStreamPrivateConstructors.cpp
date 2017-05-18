/*
 *  Copyright (C) 2015 Canon Inc. All rights reserved.
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

#include "config.h"
#include "JSReadableStreamPrivateConstructors.h"

#if ENABLE(STREAMS_API)

#include "JSDOMBuiltinConstructor.h"
#include "JSReadableByteStreamController.h"
#include "JSReadableStream.h"
#include "JSReadableStreamBYOBReader.h"
#include "JSReadableStreamBYOBRequest.h"
#include "JSReadableStreamDefaultController.h"
#include "JSReadableStreamDefaultReader.h"
#include "ReadableByteStreamInternalsBuiltins.h"
#include "ReadableStreamInternalsBuiltins.h"
#include "WebCoreJSClientData.h"
#include <runtime/JSCInlines.h>

using namespace JSC;

namespace WebCore {

enum class ReaderType {
    Byob,
    Default,
};

template <ReaderType type>
EncodedJSValue JSC_HOST_CALL constructJSReadableStreamReaderGeneric(ExecState&);

// Public JS ReadableStreamDefaultController constructor callback.
EncodedJSValue JSC_HOST_CALL constructJSReadableStreamDefaultController(ExecState& exec)
{
    VM& vm = exec.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    return throwVMTypeError(&exec, scope, ASCIILiteral("ReadableStreamDefaultController constructor should not be called directly"));
}

// Public JS ReadableStreamDefaultReader and ReadableStreamBYOBReader callbacks.
template<ReaderType type>
EncodedJSValue JSC_HOST_CALL constructJSReadableStreamReaderGeneric(ExecState& exec)
{
    VM& vm = exec.vm();
    auto& clientData = *static_cast<JSVMClientData*>(vm.clientData);
    auto& globalObject = *JSC::jsCast<JSDOMGlobalObject*>(exec.lexicalGlobalObject());

    JSC::JSObject* constructor;
    if (type == ReaderType::Byob)
        constructor = JSC::asObject(globalObject.get(&exec, clientData.builtinNames().ReadableStreamBYOBReaderPrivateName()));
    else
        constructor = JSC::asObject(globalObject.get(&exec, clientData.builtinNames().ReadableStreamDefaultReaderPrivateName()));

    ConstructData constructData;
    ConstructType constructType = constructor->methodTable(vm)->getConstructData(constructor, constructData);
    ASSERT(constructType != ConstructType::None);

    MarkedArgumentBuffer args;
    args.append(exec.argument(0));
    return JSValue::encode(JSC::construct(&exec, constructor, constructType, constructData, args));
}

EncodedJSValue JSC_HOST_CALL constructJSReadableStreamDefaultReader(ExecState& exec)
{
    return constructJSReadableStreamReaderGeneric<ReaderType::Default>(exec);
}

EncodedJSValue JSC_HOST_CALL constructJSReadableStreamBYOBReader(ExecState& exec)
{
    return constructJSReadableStreamReaderGeneric<ReaderType::Byob>(exec);
}

// Public JS ReadableByteStreamController and ReadableStreamBYOBRequest constructor callbacks.
EncodedJSValue JSC_HOST_CALL constructJSReadableByteStreamController(ExecState& exec)
{
    VM& vm = exec.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    return throwVMTypeError(&exec, scope, ASCIILiteral("ReadableByteStreamController constructor should not be called directly"));
}

EncodedJSValue JSC_HOST_CALL constructJSReadableStreamBYOBRequest(ExecState& exec)
{
    VM& vm = exec.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    return throwVMTypeError(&exec, scope, ASCIILiteral("ReadableStreamBYOBRequest constructor should not be called directly"));
}

// Private JS ReadableStreamDefaultReader and ReadableStreamDefaultController constructors.
using JSBuiltinReadableStreamDefaultReaderPrivateConstructor = JSDOMBuiltinConstructor<JSReadableStreamDefaultReader>;
using JSBuiltinReadableStreamDefaultControllerPrivateConstructor = JSDOMBuiltinConstructor<JSReadableStreamDefaultController>;
// Private JS ReadableByteStreamController, ReadableStreamBYOBReader and ReadableStreamBYOBRequest constructors.
using JSBuiltinReadableByteStreamControllerPrivateConstructor = JSDOMBuiltinConstructor<JSReadableByteStreamController>;
using JSBuiltinReadableStreamBYOBReaderPrivateConstructor = JSDOMBuiltinConstructor<JSReadableStreamBYOBReader>;
using JSBuiltinReadableStreamBYOBRequestPrivateConstructor = JSDOMBuiltinConstructor<JSReadableStreamBYOBRequest>;

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(JSBuiltinReadableStreamDefaultReaderPrivateConstructor);
STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(JSBuiltinReadableStreamDefaultControllerPrivateConstructor);
STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(JSBuiltinReadableByteStreamControllerPrivateConstructor);
STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(JSBuiltinReadableStreamBYOBReaderPrivateConstructor);
STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(JSBuiltinReadableStreamBYOBRequestPrivateConstructor);

template<> const ClassInfo JSBuiltinReadableStreamDefaultReaderPrivateConstructor::s_info = { "ReadableStreamDefaultReaderPrivateConstructor", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSBuiltinReadableStreamDefaultReaderPrivateConstructor) };
template<> const ClassInfo JSBuiltinReadableStreamDefaultControllerPrivateConstructor::s_info = { "ReadableStreamDefaultControllerPrivateConstructor", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSBuiltinReadableStreamDefaultControllerPrivateConstructor) };
template<> const ClassInfo JSBuiltinReadableByteStreamControllerPrivateConstructor::s_info = { "ReadableByteStreamControllerPrivateConstructor", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSBuiltinReadableByteStreamControllerPrivateConstructor) };
template<> const ClassInfo JSBuiltinReadableStreamBYOBReaderPrivateConstructor::s_info = { "ReadableStreamBYOBReaderPrivateConstructor", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSBuiltinReadableStreamBYOBReaderPrivateConstructor) };
template<> const ClassInfo JSBuiltinReadableStreamBYOBRequestPrivateConstructor::s_info = { "ReadableStreamBYOBRequestPrivateConstructor", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSBuiltinReadableStreamBYOBRequestPrivateConstructor) };

template<> FunctionExecutable* JSBuiltinReadableStreamDefaultReaderPrivateConstructor::initializeExecutable(JSC::VM& vm)
{
    return readableStreamInternalsPrivateInitializeReadableStreamDefaultReaderCodeGenerator(vm);
}

template<> FunctionExecutable* JSBuiltinReadableStreamDefaultControllerPrivateConstructor::initializeExecutable(JSC::VM& vm)
{
    return readableStreamInternalsPrivateInitializeReadableStreamDefaultControllerCodeGenerator(vm);
}

template<> FunctionExecutable* JSBuiltinReadableByteStreamControllerPrivateConstructor::initializeExecutable(JSC::VM& vm)
{
    return readableByteStreamInternalsPrivateInitializeReadableByteStreamControllerCodeGenerator(vm);
}

template<> FunctionExecutable* JSBuiltinReadableStreamBYOBReaderPrivateConstructor::initializeExecutable(JSC::VM& vm)
{
    return readableByteStreamInternalsPrivateInitializeReadableStreamBYOBReaderCodeGenerator(vm);
}

template<> FunctionExecutable* JSBuiltinReadableStreamBYOBRequestPrivateConstructor::initializeExecutable(JSC::VM& vm)
{
    return readableByteStreamInternalsPrivateInitializeReadableStreamBYOBRequestCodeGenerator(vm);
}

JSObject* createReadableStreamDefaultReaderPrivateConstructor(VM& vm, JSDOMGlobalObject& globalObject)
{
    return JSBuiltinReadableStreamDefaultReaderPrivateConstructor::create(vm, JSBuiltinReadableStreamDefaultReaderPrivateConstructor::createStructure(vm, globalObject, globalObject.objectPrototype()), globalObject);
}

JSObject* createReadableStreamDefaultControllerPrivateConstructor(VM& vm, JSDOMGlobalObject& globalObject)
{
    return JSBuiltinReadableStreamDefaultControllerPrivateConstructor::create(vm, JSBuiltinReadableStreamDefaultControllerPrivateConstructor::createStructure(vm, globalObject, globalObject.objectPrototype()), globalObject);
}

JSObject* createReadableByteStreamControllerPrivateConstructor(VM& vm, JSDOMGlobalObject& globalObject)
{
    return JSBuiltinReadableByteStreamControllerPrivateConstructor::create(vm, JSBuiltinReadableByteStreamControllerPrivateConstructor::createStructure(vm, globalObject, globalObject.objectPrototype()), globalObject);
}

JSObject* createReadableStreamBYOBReaderPrivateConstructor(VM& vm, JSDOMGlobalObject& globalObject)
{
    return JSBuiltinReadableStreamBYOBReaderPrivateConstructor::create(vm, JSBuiltinReadableStreamBYOBReaderPrivateConstructor::createStructure(vm, globalObject, globalObject.objectPrototype()), globalObject);
}

JSObject* createReadableStreamBYOBRequestPrivateConstructor(VM& vm, JSDOMGlobalObject& globalObject)
{
    return JSBuiltinReadableStreamBYOBRequestPrivateConstructor::create(vm, JSBuiltinReadableStreamBYOBRequestPrivateConstructor::createStructure(vm, globalObject, globalObject.objectPrototype()), globalObject);
}

} // namespace WebCore

#endif
