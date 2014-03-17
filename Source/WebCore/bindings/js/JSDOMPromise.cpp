/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "JSDOMPromise.h"

#if ENABLE(PROMISES)

using namespace JSC;

namespace WebCore {

DeferredWrapper::DeferredWrapper(ExecState* exec, JSDOMGlobalObject* globalObject)
    : m_globalObject(exec->vm(), globalObject)
    , m_deferred(exec->vm(), JSPromiseDeferred::create(exec, globalObject))
{
}

JSObject* DeferredWrapper::promise() const
{
    return m_deferred->promise();
}

void DeferredWrapper::resolve(ExecState* exec, JSValue resolution)
{
    JSValue deferredResolve = m_deferred->resolve();

    CallData resolveCallData;
    CallType resolveCallType = getCallData(deferredResolve, resolveCallData);
    ASSERT(resolveCallType != CallTypeNone);

    MarkedArgumentBuffer arguments;
    arguments.append(resolution);

    call(exec, deferredResolve, resolveCallType, resolveCallData, jsUndefined(), arguments);
}

void DeferredWrapper::reject(ExecState* exec, JSValue reason)
{
    JSValue deferredReject = m_deferred->reject();

    CallData rejectCallData;
    CallType rejectCallType = getCallData(deferredReject, rejectCallData);
    ASSERT(rejectCallType != CallTypeNone);

    MarkedArgumentBuffer arguments;
    arguments.append(reason);

    call(exec, deferredReject, rejectCallType, rejectCallData, jsUndefined(), arguments);
}

}

#endif // ENABLE(PROMISES)
