/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "V8ThrowException.h"

#include "ExceptionHeaders.h"
#include "ExceptionInterfaces.h"
#include "V8Binding.h"

namespace WebCore {

static v8::Handle<v8::Value> domExceptionStackGetter(v8::Local<v8::String> name, const v8::AccessorInfo& info)
{
    ASSERT(info.Data()->IsObject());
    return info.Data()->ToObject()->Get(v8String("stack", info.GetIsolate()));
}

static void domExceptionStackSetter(v8::Local<v8::String> name, v8::Local<v8::Value> value, const v8::AccessorInfo& info)
{
    ASSERT(info.Data()->IsObject());
    info.Data()->ToObject()->Set(v8String("stack", info.GetIsolate()), value);
}

#define TRY_TO_CREATE_EXCEPTION(interfaceName) \
    case interfaceName##Type: \
        exception = toV8(interfaceName::create(description), isolate); \
        break;

v8::Handle<v8::Value> V8ThrowException::setDOMException(int ec, v8::Isolate* isolate)
{
    if (ec <= 0 || v8::V8::IsExecutionTerminating())
        return v8Undefined();

    if (ec == NATIVE_TYPE_ERR)
        return V8ThrowException::throwTypeError(0, isolate);

    ExceptionCodeDescription description(ec);

    v8::Handle<v8::Value> exception;
    switch (description.type) {
        DOM_EXCEPTION_INTERFACES_FOR_EACH(TRY_TO_CREATE_EXCEPTION)
    }

    if (exception.IsEmpty())
        return v8Undefined();

    // Attach an Error object to the DOMException. This is then lazily used to get the stack value.
    v8::Handle<v8::Value> error = v8::Exception::Error(v8String(description.description, isolate));
    ASSERT(!error.IsEmpty());
    ASSERT(exception->IsObject());
    exception->ToObject()->SetAccessor(v8String("stack", isolate), domExceptionStackGetter, domExceptionStackSetter, error);

    return v8::ThrowException(exception);
}

#undef TRY_TO_CREATE_EXCEPTION

v8::Handle<v8::Value> V8ThrowException::throwError(ErrorType type, const char* message, v8::Isolate* isolate)
{
    switch (type) {
    case RangeError:
        return v8::ThrowException(v8::Exception::RangeError(v8String(message, isolate)));
    case ReferenceError:
        return v8::ThrowException(v8::Exception::ReferenceError(v8String(message, isolate)));
    case SyntaxError:
        return v8::ThrowException(v8::Exception::SyntaxError(v8String(message, isolate)));
    case TypeError:
        return v8::ThrowException(v8::Exception::TypeError(v8String(message, isolate)));
    case GeneralError:
        return v8::ThrowException(v8::Exception::Error(v8String(message, isolate)));
    default:
        ASSERT_NOT_REACHED();
        return v8Undefined();
    }
}

v8::Handle<v8::Value> V8ThrowException::throwTypeError(const char* message, v8::Isolate* isolate)
{
    return V8ThrowException::throwError(TypeError, (message ? message : "Type error"), isolate);
}

v8::Handle<v8::Value> V8ThrowException::throwNotEnoughArgumentsError(v8::Isolate* isolate)
{
    return V8ThrowException::throwError(TypeError, "Not enough arguments", isolate);
}

v8::Handle<v8::Value> V8ThrowException::throwError(v8::Local<v8::Value> exception, v8::Isolate* isolate)
{
    if (!v8::V8::IsExecutionTerminating())
        v8::ThrowException(exception);
    return v8::Undefined();
}

} // namespace WebCore
