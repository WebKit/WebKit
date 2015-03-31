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

#if ENABLE(STREAMS_API)
#include "ReadableStreamJSSource.h"

#include "JSDOMPromise.h"
#include "JSReadableStream.h"
#include "NotImplemented.h"
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

Ref<ReadableStreamJSSource> ReadableStreamJSSource::create(JSC::ExecState* exec)
{
    return adoptRef(*new ReadableStreamJSSource(exec));
}

ReadableStreamJSSource::ReadableStreamJSSource(JSC::ExecState* exec)
{
    if (exec->argumentCount()) {
        ASSERT_WITH_MESSAGE(exec->argument(0).isObject(), "Caller of ReadableStreamJSSource constructor should ensure that passed argument is an object.");
        // FIXME: Implement parameters support;
    }
}

void ReadableStreamJSSource::start(JSC::ExecState*)
{
    notImplemented();
}

} // namespace WebCore

#endif
