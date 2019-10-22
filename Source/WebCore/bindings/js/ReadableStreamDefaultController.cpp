/*
 * Copyright (C) 2016 Canon Inc.
 * Copyright (C) 2016-2019 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted, provided that the following conditions
 * are required to be met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Canon Inc. nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY CANON INC. AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL CANON INC. AND ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "config.h"
#include "ReadableStreamDefaultController.h"

#if ENABLE(STREAMS_API)

#include "WebCoreJSClientData.h"
#include <JavaScriptCore/CatchScope.h>
#include <JavaScriptCore/HeapInlines.h>
#include <JavaScriptCore/IdentifierInlines.h>
#include <JavaScriptCore/JSObjectInlines.h>

namespace WebCore {

static inline JSC::JSValue readableStreamCallFunction(JSC::JSGlobalObject& lexicalGlobalObject, JSC::JSValue jsFunction, JSC::JSValue thisValue, const JSC::ArgList& arguments)
{
    JSC::CallData callData;
    auto callType = JSC::getCallData(lexicalGlobalObject.vm(), jsFunction, callData);
    return call(&lexicalGlobalObject, jsFunction, callType, callData, thisValue, arguments);
}

JSC::JSValue ReadableStreamDefaultController::invoke(JSC::JSGlobalObject& lexicalGlobalObject, JSC::JSObject& object, const char* propertyName, JSC::JSValue parameter)
{
    JSC::VM& vm = lexicalGlobalObject.vm();
    JSC::JSLockHolder lock(vm);
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto function = object.get(&lexicalGlobalObject, JSC::Identifier::fromString(vm, propertyName));
    RETURN_IF_EXCEPTION(scope, JSC::JSValue());

    if (!function.isFunction(vm)) {
        if (!function.isUndefined())
            throwTypeError(&lexicalGlobalObject, scope, "ReadableStream trying to call a property that is not callable"_s);
        return JSC::jsUndefined();
    }

    JSC::MarkedArgumentBuffer arguments;
    arguments.append(parameter);
    ASSERT(!arguments.hasOverflowed());

    return readableStreamCallFunction(lexicalGlobalObject, function, &object, arguments);
}

} // namespace WebCore

#endif // ENABLE(STREAMS_API)
