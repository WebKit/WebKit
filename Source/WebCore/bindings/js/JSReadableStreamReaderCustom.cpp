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

JSValue JSReadableStreamReader::read(ExecState* exec)
{
    DeferredWrapper wrapper(exec, globalObject());
    auto successCallback = [wrapper](JSValue value) mutable {
        JSValue result = createIteratorResultObject(wrapper.promise()->globalObject()->globalExec(), value, false);
        wrapper.resolve(result);
    };
    auto endCallback = [wrapper]() mutable {
        JSValue result = createIteratorResultObject(wrapper.promise()->globalObject()->globalExec(), JSC::jsUndefined(), true);
        wrapper.resolve(result);
    };
    auto failureCallback = [wrapper](JSValue value) mutable {
        wrapper.reject(value);
    };

    impl().read(WTF::move(successCallback), WTF::move(endCallback), WTF::move(failureCallback));

    return wrapper.promise();
}

JSValue JSReadableStreamReader::closed(ExecState* exec) const
{
    if (!m_closedPromiseDeferred)
        const_cast<JSReadableStreamReader*>(this)->m_closedPromiseDeferred.set(exec->vm(), JSPromiseDeferred::create(exec, globalObject()));
    DeferredWrapper wrapper(exec, globalObject(), m_closedPromiseDeferred.get());

    auto successCallback = [wrapper]() mutable {
        wrapper.resolve(jsUndefined());
    };
    auto failureCallback = [wrapper](JSValue value) mutable {
        wrapper.reject(value);
    };

    impl().closed(WTF::move(successCallback), WTF::move(failureCallback));

    return wrapper.promise();
}

JSValue JSReadableStreamReader::cancel(ExecState* exec)
{
    JSValue error = createError(exec, ASCIILiteral("cancel is not implemented"));
    return exec->vm().throwException(exec, error);
}

JSValue JSReadableStreamReader::releaseLock(ExecState* exec)
{
    JSValue error = createError(exec, ASCIILiteral("releaseLock is not implemented"));
    return exec->vm().throwException(exec, error);
}

EncodedJSValue JSC_HOST_CALL constructJSReadableStreamReader(ExecState* exec)
{
    if (!exec->argumentCount())
        return throwVMError(exec, createTypeError(exec, ASCIILiteral("ReadableStreamReader constructor takes a ReadableStream as parameter")));

    JSReadableStream* stream = jsDynamicCast<JSReadableStream*>(exec->argument(0));
    if (!stream)
        return throwVMError(exec, createTypeError(exec, ASCIILiteral("ReadableStreamReader constructor parameter is not a ReadableStream")));

    if (stream->impl().isLocked())
        return throwVMError(exec, createTypeError(exec, ASCIILiteral("ReadableStreamReader constructor parameter is a locked ReadableStream")));

    return JSValue::encode(toJS(exec, stream->globalObject(), stream->impl().getReader()));
}

} // namespace WebCore

#endif
