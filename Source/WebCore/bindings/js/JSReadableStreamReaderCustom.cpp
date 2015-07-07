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
#include "JSReadableStreamReader.h"

#include "ExceptionCode.h"
#include "JSDOMPromise.h"
#include "JSReadableStream.h"
#include "ReadableJSStream.h"
#include <runtime/Error.h>
#include <runtime/IteratorOperations.h>
#include <wtf/NeverDestroyed.h>

using namespace JSC;

namespace WebCore {

JSValue JSReadableStreamReader::closed(ExecState* exec) const
{
    if (m_closedPromiseDeferred)
        return m_closedPromiseDeferred->promise();

    const_cast<JSReadableStreamReader*>(this)->m_closedPromiseDeferred.set(exec->vm(), JSPromiseDeferred::create(exec, globalObject()));
    impl().closed(DeferredWrapper(exec, globalObject(), m_closedPromiseDeferred.get()));

    return m_closedPromiseDeferred->promise();
}

EncodedJSValue JSC_HOST_CALL constructJSReadableStreamReader(ExecState* exec)
{
    if (!exec->argumentCount())
        return throwVMError(exec, createTypeError(exec, ASCIILiteral("ReadableStreamReader constructor takes a ReadableStream as parameter")));

    JSReadableStream* stream = jsDynamicCast<JSReadableStream*>(exec->argument(0));
    if (!stream)
        return throwVMError(exec, createTypeError(exec, ASCIILiteral("ReadableStreamReader constructor parameter is not a ReadableStream")));

    if (stream->impl().locked())
        return throwVMError(exec, createTypeError(exec, ASCIILiteral("ReadableStreamReader constructor parameter is a locked ReadableStream")));

    ExceptionCode ec = 0;
    EncodedJSValue value = JSValue::encode(toJS(exec, stream->globalObject(), stream->impl().getReader(ec)));
    ASSERT(!ec);
    return value;
}

} // namespace WebCore

#endif
