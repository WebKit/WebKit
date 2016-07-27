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

#include "config.h"
#include "JSReadableStreamPrivateConstructors.h"

#if ENABLE(STREAMS_API)

#include "JSDOMBinding.h"
#include "JSDOMConstructor.h"
#include "JSReadableStream.h"
#include "JSReadableStreamController.h"
#include "JSReadableStreamDefaultReader.h"
#include "ReadableStreamInternalsBuiltins.h"
#include <runtime/CallData.h>

using namespace JSC;

namespace WebCore {

// Public JS ReadableStreamReder and ReadableStreamController constructor callbacks.
EncodedJSValue JSC_HOST_CALL constructJSReadableStreamController(ExecState& exec)
{
    return throwVMTypeError(&exec, ASCIILiteral("ReadableStreamController constructor should not be called directly"));
}

EncodedJSValue JSC_HOST_CALL constructJSReadableStreamDefaultReader(ExecState& exec)
{
    JSReadableStream* stream = jsDynamicCast<JSReadableStream*>(exec.argument(0));
    if (!stream)
        return throwVMTypeError(&exec, ASCIILiteral("ReadableStreamDefaultReader constructor parameter is not a ReadableStream"));

    JSValue jsFunction = stream->get(&exec, Identifier::fromString(&exec, "getReader"));

    CallData callData;
    CallType callType = getCallData(jsFunction, callData);
    MarkedArgumentBuffer noArguments;
    return JSValue::encode(call(&exec, jsFunction, callType, callData, stream, noArguments));
}

// Private JS ReadableStreamReder and ReadableStreamController constructors.
typedef JSBuiltinConstructor<JSReadableStreamDefaultReader> JSBuiltinReadableStreamDefaultReaderPrivateConstructor;
typedef JSBuiltinConstructor<JSReadableStreamController> JSBuiltinReadableStreamControllerPrivateConstructor;

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(JSBuiltinReadableStreamDefaultReaderPrivateConstructor);
STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(JSBuiltinReadableStreamControllerPrivateConstructor);

template<> const ClassInfo JSBuiltinReadableStreamDefaultReaderPrivateConstructor::s_info = { "ReadableStreamDefaultReaderPrivateConstructor", &Base::s_info, 0, CREATE_METHOD_TABLE(JSBuiltinReadableStreamDefaultReaderPrivateConstructor) };
template<> const ClassInfo JSBuiltinReadableStreamControllerPrivateConstructor::s_info = { "ReadableStreamControllerPrivateConstructor", &Base::s_info, 0, CREATE_METHOD_TABLE(JSBuiltinReadableStreamControllerPrivateConstructor) };

template<> FunctionExecutable* JSBuiltinReadableStreamDefaultReaderPrivateConstructor::initializeExecutable(JSC::VM& vm)
{
    return readableStreamInternalsPrivateInitializeReadableStreamDefaultReaderCodeGenerator(vm);
}

template<> FunctionExecutable* JSBuiltinReadableStreamControllerPrivateConstructor::initializeExecutable(JSC::VM& vm)
{
    return readableStreamInternalsPrivateInitializeReadableStreamControllerCodeGenerator(vm);
}

JSObject* createReadableStreamDefaultReaderPrivateConstructor(VM& vm, JSDOMGlobalObject& globalObject)
{
    return JSBuiltinReadableStreamDefaultReaderPrivateConstructor::create(vm, JSBuiltinReadableStreamDefaultReaderPrivateConstructor::createStructure(vm, globalObject, globalObject.objectPrototype()), globalObject);
}

JSObject* createReadableStreamControllerPrivateConstructor(VM& vm, JSDOMGlobalObject& globalObject)
{
    return JSBuiltinReadableStreamControllerPrivateConstructor::create(vm, JSBuiltinReadableStreamControllerPrivateConstructor::createStructure(vm, globalObject, globalObject.objectPrototype()), globalObject);
}

} // namespace WebCore

#endif
