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
#include "ReadableStreamJSSource.h"

#if ENABLE(STREAMS_API)

#include "DOMWrapperWorld.h"
#include "JSDOMPromise.h"
#include "JSReadableStream.h"
#include "NotImplemented.h"
#include "ScriptExecutionContext.h"
#include <runtime/Error.h>
#include <runtime/JSCJSValueInlines.h>
#include <runtime/JSString.h>
#include <runtime/StructureInlines.h>

using namespace JSC;

namespace WebCore {

void setInternalSlotToObject(ExecState* exec, JSValue objectValue, PrivateName& name, JSValue value)
{
    JSObject* object = objectValue.toObject(exec);
    PutPropertySlot propertySlot(objectValue);
    object->put(object, exec, Identifier::fromUid(name), value, propertySlot);
}

JSValue getInternalSlotFromObject(ExecState* exec, JSValue objectValue, PrivateName& name)
{
    JSObject* object = objectValue.toObject(exec);
    PropertySlot propertySlot(objectValue);

    Identifier propertyName = Identifier::fromUid(name);
    if (!object->getOwnPropertySlot(object, exec, propertyName, propertySlot))
        return JSValue();
    return propertySlot.getValue(exec, propertyName);
}

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

Ref<ReadableStreamJSSource> ReadableStreamJSSource::create(JSC::ExecState* exec)
{
    return adoptRef(*new ReadableStreamJSSource(exec));
}

ReadableStreamJSSource::ReadableStreamJSSource(JSC::ExecState* exec)
{
    ASSERT_WITH_MESSAGE(!exec->argumentCount() || exec->argument(0).isObject(), "Caller of ReadableStreamJSSource constructor should ensure that passed argument if any is an object.");
    JSObject* source =  exec->argumentCount() ? exec->argument(0).getObject() : JSFinalObject::create(exec->vm(), JSFinalObject::createStructure(exec->vm(), exec->callee()->globalObject(), jsNull(), 1));
    m_source.set(exec->vm(), source);
}

ReadableStreamJSSource::~ReadableStreamJSSource()
{
    if (m_controller)
        m_controller.get()->impl().resetStream();
}

static void startReadableStreamAsync(ReadableStream& readableStream)
{
    RefPtr<ReadableStream> stream = &readableStream;
    stream->scriptExecutionContext()->postTask([stream](ScriptExecutionContext&) {
        stream->start();
    });
}

void ReadableStreamJSSource::start(JSC::ExecState* exec, JSReadableStream* readableStream)
{
    JSLockHolder lock(exec);

    Ref<ReadableStreamController> controller = ReadableStreamController::create(static_cast<ReadableJSStream&>(readableStream->impl()));
    m_controller.set(exec->vm(), jsDynamicCast<JSReadableStreamController*>(toJS(exec, readableStream->globalObject(), controller)));

    JSValue startFunction = getPropertyFromObject(exec, m_source.get(), "start");
    if (!startFunction.isFunction()) {
        if (!startFunction.isUndefined())
            throwVMError(exec, createTypeError(exec, ASCIILiteral("ReadableStream constructor object start property should be a function.")));
        else
            startReadableStreamAsync(readableStream->impl());
        return;
    }

    MarkedArgumentBuffer arguments;
    arguments.append(m_controller.get());

    JSValue exception;
    callFunction(exec, startFunction, m_source.get(), arguments, &exception);

    if (exception) {
        throwVMError(exec, exception);
        return;
    }

    // FIXME: Implement handling promise as result of calling start function.
    startReadableStreamAsync(readableStream->impl());
}

Ref<ReadableJSStream> ReadableJSStream::create(ScriptExecutionContext& scriptExecutionContext, Ref<ReadableStreamJSSource>&& source)
{
    auto readableStream = adoptRef(*new ReadableJSStream(scriptExecutionContext, WTF::move(source)));
    return readableStream;
}

Ref<ReadableStreamReader> ReadableJSStream::createReader()
{
    RefPtr<ReadableStreamReader> reader = ReadableJSStreamReader::create(*this);
    return reader.releaseNonNull();
}

ReadableJSStream::ReadableJSStream(ScriptExecutionContext& scriptExecutionContext, Ref<ReadableStreamJSSource>&& source)
    : ReadableStream(scriptExecutionContext, WTF::move(source))
{
}

Ref<ReadableJSStreamReader> ReadableJSStreamReader::create(ReadableJSStream& stream)
{
    auto readableStreamReader = adoptRef(*new ReadableJSStreamReader(stream));
    return readableStreamReader;
}

ReadableJSStreamReader::ReadableJSStreamReader(ReadableJSStream& readableStream)
    : ReadableStreamReader(readableStream)
{
}

} // namespace WebCore

#endif
