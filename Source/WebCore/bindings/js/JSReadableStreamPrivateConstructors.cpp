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

#include "JSDOMBinding.h"
#include "JSDOMConstructor.h"
#include "JSReadableStream.h"
#include "JSReadableStreamDefaultController.h"
#include "JSReadableStreamDefaultReader.h"
#include "ReadableStreamInternalsBuiltins.h"
#include <runtime/CallData.h>

using namespace JSC;

namespace WebCore {

// Public JS ReadableStreamReder and ReadableStreamDefaultController constructor callbacks.
EncodedJSValue JSC_HOST_CALL constructJSReadableStreamDefaultController(ExecState& exec)
{
    VM& vm = exec.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    return throwVMTypeError(&exec, scope, ASCIILiteral("ReadableStreamDefaultController constructor should not be called directly"));
}

EncodedJSValue JSC_HOST_CALL constructJSReadableStreamDefaultReader(ExecState& exec)
{
    VM& vm = exec.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    
    JSReadableStream* stream = jsDynamicCast<JSReadableStream*>(exec.argument(0));
    if (!stream)
        return throwArgumentTypeError(exec, scope, 0, "stream", "ReadableStreamReader", nullptr, "ReadableStream");

    JSValue jsFunction = stream->get(&exec, Identifier::fromString(&exec, "getReader"));

    CallData callData;
    CallType callType = getCallData(jsFunction, callData);
    MarkedArgumentBuffer noArguments;
    return JSValue::encode(call(&exec, jsFunction, callType, callData, stream, noArguments));
}

// Private JS ReadableStreamDefaultReader and ReadableStreamDefaultController constructors.
typedef JSBuiltinConstructor<JSReadableStreamDefaultReader> JSBuiltinReadableStreamDefaultReaderPrivateConstructor;
typedef JSBuiltinConstructor<JSReadableStreamDefaultController> JSBuiltinReadableStreamDefaultControllerPrivateConstructor;

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(JSBuiltinReadableStreamDefaultReaderPrivateConstructor);
STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(JSBuiltinReadableStreamDefaultControllerPrivateConstructor);

template<> const ClassInfo JSBuiltinReadableStreamDefaultReaderPrivateConstructor::s_info = { "ReadableStreamDefaultReaderPrivateConstructor", &Base::s_info, 0, CREATE_METHOD_TABLE(JSBuiltinReadableStreamDefaultReaderPrivateConstructor) };
template<> const ClassInfo JSBuiltinReadableStreamDefaultControllerPrivateConstructor::s_info = { "ReadableStreamDefaultControllerPrivateConstructor", &Base::s_info, 0, CREATE_METHOD_TABLE(JSBuiltinReadableStreamDefaultControllerPrivateConstructor) };

template<> FunctionExecutable* JSBuiltinReadableStreamDefaultReaderPrivateConstructor::initializeExecutable(JSC::VM& vm)
{
    return readableStreamInternalsPrivateInitializeReadableStreamDefaultReaderCodeGenerator(vm);
}

template<> FunctionExecutable* JSBuiltinReadableStreamDefaultControllerPrivateConstructor::initializeExecutable(JSC::VM& vm)
{
    return readableStreamInternalsPrivateInitializeReadableStreamDefaultControllerCodeGenerator(vm);
}

JSObject* createReadableStreamDefaultReaderPrivateConstructor(VM& vm, JSDOMGlobalObject& globalObject)
{
    return JSBuiltinReadableStreamDefaultReaderPrivateConstructor::create(vm, JSBuiltinReadableStreamDefaultReaderPrivateConstructor::createStructure(vm, globalObject, globalObject.objectPrototype()), globalObject);
}

JSObject* createReadableStreamDefaultControllerPrivateConstructor(VM& vm, JSDOMGlobalObject& globalObject)
{
    return JSBuiltinReadableStreamDefaultControllerPrivateConstructor::create(vm, JSBuiltinReadableStreamDefaultControllerPrivateConstructor::createStructure(vm, globalObject, globalObject.objectPrototype()), globalObject);
}

} // namespace WebCore

#endif
