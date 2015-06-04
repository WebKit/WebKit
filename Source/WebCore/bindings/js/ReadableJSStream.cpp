/*
 * Copyright (C) 2015 Canon Inc.
 * Copyright (C) 2015 Igalia S.L.
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
#include "ReadableJSStream.h"

#if ENABLE(STREAMS_API)

#include "DOMWrapperWorld.h"
#include "JSDOMPromise.h"
#include "JSReadableStream.h"
#include "JSReadableStreamController.h"
#include "NotImplemented.h"
#include "ScriptExecutionContext.h"
#include <runtime/Error.h>
#include <runtime/JSCJSValueInlines.h>
#include <runtime/JSString.h>
#include <runtime/StructureInlines.h>

using namespace JSC;

namespace WebCore {

static inline JSValue getPropertyFromObject(ExecState* exec, JSObject* object, const char* identifier)
{
    return object->get(exec, Identifier::fromString(exec, identifier));
}

static inline JSValue callFunction(ExecState* exec, JSValue jsFunction, JSValue thisValue, const ArgList& arguments, JSValue* exception)
{
    CallData callData;
    CallType callType = getCallData(jsFunction, callData);
    return call(exec, jsFunction, callType, callData, thisValue, arguments, exception);
}

JSDOMGlobalObject* ReadableJSStream::globalObject()
{
    return jsDynamicCast<JSDOMGlobalObject*>(m_source->globalObject());
}

static void startReadableStreamAsync(ReadableStream& readableStream)
{
    RefPtr<ReadableStream> stream = &readableStream;
    stream->scriptExecutionContext()->postTask([stream](ScriptExecutionContext&) {
        stream->start();
    });
}

void ReadableJSStream::doStart(ExecState& exec)
{
    JSLockHolder lock(&exec);

    JSValue startFunction = getPropertyFromObject(&exec, m_source.get(), "start");
    if (!startFunction.isFunction()) {
        if (!startFunction.isUndefined())
            throwVMError(&exec, createTypeError(&exec, ASCIILiteral("ReadableStream constructor object start property should be a function.")));
        else
            startReadableStreamAsync(*this);
        return;
    }

    MarkedArgumentBuffer arguments;
    arguments.append(jsController(exec, globalObject()));

    JSValue exception;
    callFunction(&exec, startFunction, m_source.get(), arguments, &exception);

    if (exception) {
        throwVMError(&exec, exception);
        return;
    }

    // FIXME: Implement handling promise as result of calling start function.
    startReadableStreamAsync(*this);
}

Ref<ReadableJSStream> ReadableJSStream::create(ExecState& exec, ScriptExecutionContext& scriptExecutionContext)
{
    ASSERT_WITH_MESSAGE(!exec.argumentCount() || exec.argument(0).isObject(), "Caller of ReadableJSStream constructor should ensure that passed argument if any is an object.");
    JSObject* source =  exec.argumentCount() ? exec.argument(0).getObject() : JSFinalObject::create(exec.vm(), JSFinalObject::createStructure(exec.vm(), exec.callee()->globalObject(), jsNull(), 1));

    Ref<ReadableJSStream> readableStream = adoptRef(*new ReadableJSStream(scriptExecutionContext, exec, source));
    readableStream->doStart(exec);
    return readableStream;
}

ReadableJSStream::ReadableJSStream(ScriptExecutionContext& scriptExecutionContext, ExecState& exec, JSObject* source)
    : ReadableStream(scriptExecutionContext)
{
    m_source.set(exec.vm(), source);
}

JSValue ReadableJSStream::jsController(ExecState& exec, JSDOMGlobalObject* globalObject)
{
    if (!m_controller)
        m_controller = std::make_unique<ReadableStreamController>(*this);
    return toJS(&exec, globalObject, m_controller.get());
}

void ReadableJSStream::storeError(JSC::ExecState& exec)
{
    if (m_error)
        return;
    JSValue error = exec.argumentCount() ? exec.argument(0) : createError(&exec, ASCIILiteral("Error function called."));
    m_error.set(exec.vm(), error);

    changeStateToErrored();
}

bool ReadableJSStream::hasValue() const
{
    notImplemented();
    return false;
}

JSValue ReadableJSStream::read()
{
    notImplemented();
    return jsUndefined();
}

} // namespace WebCore

#endif
