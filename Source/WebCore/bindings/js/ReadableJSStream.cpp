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

static inline JSValue getPropertyFromObject(ExecState& exec, JSObject* object, const char* identifier)
{
    return object->get(&exec, Identifier::fromString(&exec, identifier));
}

static inline JSValue callFunction(ExecState& exec, JSValue jsFunction, JSValue thisValue, const ArgList& arguments)
{
    CallData callData;
    CallType callType = getCallData(jsFunction, callData);
    return call(&exec, jsFunction, callType, callData, thisValue, arguments);
}

JSValue ReadableJSStream::invoke(ExecState& exec, const char* propertyName)
{
    JSValue function = getPropertyFromObject(exec, m_source.get(), propertyName);
    if (exec.hadException())
        return jsUndefined();

    if (!function.isFunction()) {
        if (!function.isUndefined())
            throwVMError(&exec, createTypeError(&exec, ASCIILiteral("ReadableStream trying to call a property that is not callable")));
        return jsUndefined();
    }

    MarkedArgumentBuffer arguments;
    arguments.append(jsController(exec, globalObject()));
    return callFunction(exec, function, m_source.get(), arguments);
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

    invoke(exec, "start");

    if (exec.hadException())
        return;

    // FIXME: Implement handling promise as result of calling start function.
    startReadableStreamAsync(*this);
}

RefPtr<ReadableJSStream> ReadableJSStream::create(ExecState& exec, ScriptExecutionContext& scriptExecutionContext)
{
    JSObject* jsSource;
    JSValue value = exec.argument(0);
    if (value.isObject())
        jsSource = value.getObject();
    else if (!value.isUndefined()) {
        throwVMError(&exec, createTypeError(&exec, ASCIILiteral("ReadableStream constructor first argument, if any, should be an object")));
        return nullptr;
    } else
        jsSource = JSFinalObject::create(exec.vm(), JSFinalObject::createStructure(exec.vm(), exec.callee()->globalObject(), jsNull(), 1));

    RefPtr<ReadableJSStream> readableStream = adoptRef(*new ReadableJSStream(scriptExecutionContext, exec, jsSource));
    readableStream->doStart(exec);

    if (exec.hadException())
        return nullptr;

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
    return m_chunkQueue.size();
}

JSValue ReadableJSStream::read()
{
    ASSERT(hasValue());

    return m_chunkQueue.takeFirst().get();
}

void ReadableJSStream::enqueue(ExecState& exec)
{
    ASSERT(!isCloseRequested());

    if (!isReadable())
        return;

    JSValue chunk = exec.argumentCount() ? exec.argument(0) : jsUndefined();
    if (resolveReadCallback(chunk))
        return;

    m_chunkQueue.append(JSC::Strong<JSC::Unknown>(exec.vm(), chunk));
    // FIXME: Compute chunk size.
    // FIXME: Add pulling of data here and also when data is passed to resolve callback.
}

} // namespace WebCore

#endif
